#include "queue.h"
#include <lib/kalloc.h>
#include <lib/log.h>
#include <lib/memory.h>
#include <lib/print.h>
#include <page_table.h>

static inline log_t *virtio_q_log() {
  static log_t *l = NULL;
  if (!l) {
    l = g_log_create("virtio", "queue");
#if VIRTIO_DEBUG
    g_log_set_level(l, LOG_LEVEL_DEBUG);
#else
    g_log_set_level(l, LOG_LEVEL_INFO);
#endif
  }
  return l;
}

#include "mmio.h"
#include <device/virtio/virtio.h>

typedef struct virtq_internal_header {
  void **cookies; // per-desc head cookie mapping
  uint16_t size;
} virtq_internal_header_t;

static inline void write_barrier(void) { __sync_synchronize(); }
static inline void read_barrier(void) { __sync_synchronize(); }

static size_t ring_bytes_avail(uint16_t size) {
  return sizeof(struct virtq_avail) + sizeof(uint16_t) * size;
}
static size_t ring_bytes_used(uint16_t size) {
  return sizeof(struct virtq_used) + sizeof(struct virtq_used_elem) * size;
}

int virtq_create(virtio_device_t *dev, uint16_t qidx, uint16_t requested_size,
                 struct virtq **out) {
  if (!dev || !out)
    return -1;
  uint32_t max = virtio_mmio_read32(dev->mmio_base, VIRTIO_MMIO_QUEUE_NUM_MAX);
  if (max == 0)
    return -2;
  uint16_t size = requested_size;
  if (size == 0 || size > max)
    size = (uint16_t)max;
#if VIRTIO_DEBUG
  LOG_DEBUG(virtio_q_log(),
            "q%{type: int}: create size=%{type: int} (max=%{type: int})", qidx,
            size, max);
#endif

  virtio_mmio_write32(dev->mmio_base, VIRTIO_MMIO_QUEUE_SEL, qidx);
  virtio_mmio_write32(dev->mmio_base, VIRTIO_MMIO_QUEUE_NUM, size);

  size_t desc_bytes = sizeof(struct virtq_desc) * size;
  size_t avail_bytes = ring_bytes_avail(size);
  size_t used_bytes = ring_bytes_used(size);

  struct virtq_desc *desc = (struct virtq_desc *)kalloc(desc_bytes);
  struct virtq_avail *avail = (struct virtq_avail *)kalloc(avail_bytes);
  struct virtq_used *used = (struct virtq_used *)kalloc(used_bytes);
  if (!desc || !avail || !used)
    return -3;
  memset(desc, 0, desc_bytes);
  memset(avail, 0, avail_bytes);
  memset(used, 0, used_bytes);

  uintptr_t p_desc = V2P((uintptr_t)desc);
  uintptr_t p_avail = V2P((uintptr_t)avail);
  uintptr_t p_used = V2P((uintptr_t)used);

  virtio_mmio_write32(dev->mmio_base, VIRTIO_MMIO_QUEUE_DESC_LOW,
                      (uint32_t)(p_desc & 0xFFFFFFFFu));
  virtio_mmio_write32(dev->mmio_base, VIRTIO_MMIO_QUEUE_DESC_HIGH,
                      (uint32_t)(p_desc >> 32));
  virtio_mmio_write32(dev->mmio_base, VIRTIO_MMIO_DRIVER_DESC_LOW,
                      (uint32_t)(p_avail & 0xFFFFFFFFu));
  virtio_mmio_write32(dev->mmio_base, VIRTIO_MMIO_DRIVER_DESC_HIGH,
                      (uint32_t)(p_avail >> 32));
  virtio_mmio_write32(dev->mmio_base, VIRTIO_MMIO_DEVICE_DESC_LOW,
                      (uint32_t)(p_used & 0xFFFFFFFFu));
  virtio_mmio_write32(dev->mmio_base, VIRTIO_MMIO_DEVICE_DESC_HIGH,
                      (uint32_t)(p_used >> 32));

  virtio_mmio_write32(dev->mmio_base, VIRTIO_MMIO_QUEUE_READY, 1);
#if VIRTIO_DEBUG
  LOG_DEBUG(virtio_q_log(),
            "q%{type: int}: programmed ring desc=%{type: hex} avail=%{type: "
            "hex} used=%{type: hex}",
            qidx, (uint64_t)p_desc, (uint64_t)p_avail, (uint64_t)p_used);
#endif

  struct virtq *q = (struct virtq *)kalloc(sizeof(struct virtq));
  if (!q)
    return -4;
  memset(q, 0, sizeof(struct virtq));
  q->desc = desc;
  q->avail = avail;
  q->used = used;
  q->size = size;
  q->last_used_idx = 0;
  initlock(&q->lock, "virtq");
  q->dev = dev;
  q->qidx = qidx;
  if (qidx < (uint16_t)(sizeof(dev->queues) / sizeof(dev->queues[0]))) {
    dev->queues[qidx] = q;
    if (qidx + 1 > dev->num_queues)
      dev->num_queues = (uint16_t)(qidx + 1);
  }

  // Allocate cookie array (separate page)
  virtq_internal_header_t *hdr =
      (virtq_internal_header_t *)kalloc(sizeof(virtq_internal_header_t));
  if (!hdr)
    return -5;
  hdr->cookies = (void **)kalloc(sizeof(void *) * size);
  if (!hdr->cookies)
    return -6;
  memset(hdr->cookies, 0, sizeof(void *) * size);
  hdr->size = size;
  // Store header pointer just before desc (opaque to API users)
  // Caller can manage cookies externally as needed; we keep a side array here.
  (void)desc_bytes;
  (void)avail_bytes;
  (void)used_bytes; // silence if unused

  *out = (struct virtq *)q;
  return 0;
}

int virtq_kick(virtio_device_t *dev, uint16_t qidx) {
  virtio_mmio_write32(dev->mmio_base, VIRTIO_MMIO_QUEUE_NOTIFY, qidx);
#if VIRTIO_DEBUG
  LOG_DEBUG(virtio_q_log(), "q%{type: int}: kick", qidx);
#endif
  return 0;
}

// Minimal poll; a full cookie map needs an out-of-band association by caller.
int virtq_poll_used(struct virtq *q, void **out_cookie, size_t *out_bytes) {
  if (!q)
    return -1;
  read_barrier();

  // Protect last_used_idx and ring observation with the queue lock.
  // Release before invoking callbacks to avoid deadlocks with virtq_submit.
  acquire(&q->lock);
  if (q->last_used_idx == q->used->idx) {
    release(&q->lock);
    return 1; // no completion
  }
  uint16_t pos = q->last_used_idx % q->size;
  uint32_t id = q->used->ring[pos].id;
  uint32_t len = q->used->ring[pos].len;
  q->last_used_idx++;
  release(&q->lock);

  if (out_cookie)
    *out_cookie = (void *)(uintptr_t)id; // head id as cookie
  if (out_bytes)
    *out_bytes = len;
  if (q->on_used)
    q->on_used(q, (void *)(uintptr_t)id, len, q->cb_user);
  return 0;
}

// Submit uses head-id as cookie by default. A higher-level wrapper should map
// ids to real cookies if needed.
int virtq_submit(struct virtq *q, const struct iovec *out_sg, size_t out_cnt,
                 const struct iovec *in_sg, size_t in_cnt, void *cookie) {
  if (!q)
    return -1;
  acquire(&q->lock);
  // Choose head descriptor index.
  // For single-descriptor submissions, allow caller to specify the descriptor
  // index via cookie (used by input driver to pre-post distinct event slots).
  uint16_t head = 0; // default
  uint16_t total_desc = (uint16_t)(out_cnt + in_cnt);
  if (total_desc == 1 && cookie) {
    uintptr_t idx = (uintptr_t)cookie;
    if (idx < q->size) {
      head = (uint16_t)idx;
    }
  }
  uint16_t cur = head;

  // Build OUT descriptors (device reads)
  for (size_t i = 0; i < out_cnt; i++) {
    q->desc[cur].addr = V2P((uintptr_t)out_sg[i].iov_base);
    q->desc[cur].len = (uint32_t)out_sg[i].iov_len;
    q->desc[cur].flags =
        (i + 1 < out_cnt || in_cnt > 0) ? VRING_DESC_F_NEXT : 0;
    if (q->desc[cur].flags & VRING_DESC_F_NEXT)
      q->desc[cur].next = (uint16_t)(cur + 1);
    cur++;
  }

  // Build IN descriptors (device writes)
  for (size_t i = 0; i < in_cnt; i++) {
    q->desc[cur].addr = V2P((uintptr_t)in_sg[i].iov_base);
    q->desc[cur].len = (uint32_t)in_sg[i].iov_len;
    q->desc[cur].flags =
        VRING_DESC_F_WRITE | ((i + 1 < in_cnt) ? VRING_DESC_F_NEXT : 0);
    if (q->desc[cur].flags & VRING_DESC_F_NEXT)
      q->desc[cur].next = (uint16_t)(cur + 1);
    cur++;
  }

  // Post to avail ring
  uint16_t idx = q->avail->idx;
  q->avail->ring[idx % q->size] = head;
  write_barrier();
  q->avail->idx = (uint16_t)(idx + 1);
  release(&q->lock);
  (void)cookie; // cookie is carried via head id; suppress unused warnings in
                // other paths
#if VIRTIO_DEBUG
  LOG_DEBUG(virtio_q_log(),
            "q%{type: int}: submit head=%{type: int} out=%{type: int} "
            "in=%{type: int}",
            q->qidx, head, (int)out_cnt, (int)in_cnt);
#endif
  return 0;
}

// Drain completions for a device's queues (simple scan for now)
void virtq_handle_irq_for_device(virtio_device_t *dev) {
  (void)dev;
  // In this minimal version, drivers call virtq_poll_used periodically or from
  // ISR
}

void virtq_set_callback(struct virtq *q,
                        void (*on_used)(struct virtq *, void *cookie,
                                        size_t bytes, void *user),
                        void *user) {
  q->on_used = on_used;
  q->cb_user = user;
}
