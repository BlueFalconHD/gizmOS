#include "virtio_gpu.h"
#include <lib/memory.h>
#include <lib/panic.h>
#include <lib/print.h>
#include <page_table.h>
#include <physical_alloc.h>

/* how many descriptors we want for controlq (power-of-two) */
#define GPUQ_SZ 64

/* ------------------------------------------------------------------ helpers */
static inline uint64_t p2p(void *v) { return V2P((uint64_t)v); } /* virt→phys */

/* allocate a descriptor from q->free_map (very small spin helper) */
static int alloc_desc(virtio_queue_t *q) {
  for (int i = 0; i < q->size; i++)
    if (q->free_map[i]) {
      q->free_map[i] = 0;
      return i;
    }
  return -1;
}
static void free_chain(virtio_queue_t *q, int head) {
  while (1) {
    q->free_map[head] = 1;
    if (!(q->desc[head].flags & VRING_DESC_F_NEXT))
      break;
    head = q->desc[head].next;
  }
}

/* enqueue one OUT + (optional) IN buffer, notify, return head index */
static int send_cmd(virtio_device_t *vd, virtio_queue_t *q, void *out,
                    uint32_t out_len, void *in, uint32_t in_len,
                    uint16_t qsel) {
  int d0 = alloc_desc(q);
  if (d0 < 0)
    return -1;
  q->desc[d0].addr = p2p(out);
  q->desc[d0].len = out_len;
  q->desc[d0].flags = 0; /* device reads */

  int din = -1;
  if (in && in_len) {
    din = alloc_desc(q);
    if (din < 0) {
      q->free_map[d0] = 1;
      return -1;
    }
    q->desc[d0].flags |= VRING_DESC_F_NEXT;
    q->desc[d0].next = din;
    q->desc[din].addr = p2p(in);
    q->desc[din].len = in_len;
    q->desc[din].flags = VRING_DESC_F_WRITE; /* device writes */
  }

  uint16_t slot = q->avail->idx % q->size;
  q->avail->ring[slot] = d0;
  __sync_synchronize();
  q->avail->idx++;

  virtio_mmio_write(vd, VIRTIO_MMIO_QUEUE_NOTIFY, qsel);
  return d0;
}

/* wait until ‘head’ shows up in used ring, then free chain */
static void wait_cmd(virtio_device_t *vd, virtio_queue_t *q, int head) {
  while (q->last_used_idx == q->used->idx) { /* busy-wait */
  }
  while (q->last_used_idx != q->used->idx) {
    int id = q->used->ring[q->last_used_idx % q->size].id;
    q->last_used_idx++;
    free_chain(q, id);
    if (id == head)
      break;
  }
}

/* ------------------------------------------------------------------ ctor/init
 */
RESULT_TYPE(virtio_gpu_t *) make_virtio_gpu(uint64_t base, uint32_t irq) {
  virtio_gpu_t *g = (virtio_gpu_t *)alloc_page();
  if (!g)
    return RESULT_FAILURE(RESULT_NOMEM);

  g->vdev.base = base;
  g->vdev.irq = irq;
  g->vdev.is_initialized = 0;
  return RESULT_SUCCESS(g);
}

g_bool virtio_gpu_init(virtio_gpu_t *g) {
  if (!g)
    return false;
  /* 1. generic device-level init (accept VERSION_1 only) */
  if (!virtio_device_init(&g->vdev, VIRTIO_F_VERSION_1))
    return false;

  /* 2. set up control-queue 0 – but **without** pre-posted buffers */
  if (!virtio_queue_setup(&g->vdev, &g->q_ctrl,
                          /*qsel*/ 0, GPUQ_SZ,
                          /*buffers*/ (void *)0xdeadbeef, /*dummy*/
                          /*elem_size*/ 0))
    return false;
  /* Undo the priming the helper did: mark all descriptors free & idx=0 */
  memset(g->q_ctrl.free_map, 1, GPUQ_SZ);
  g->q_ctrl.avail->idx = g->q_ctrl.used->idx = g->q_ctrl.last_used_idx = 0;

  /* 3. DEVICE_OK so the card accepts commands */
  virtio_mmio_write(&g->vdev, VIRTIO_MMIO_STATUS,
                    virtio_mmio_read(&g->vdev, VIRTIO_MMIO_STATUS) |
                        VIRTIO_CONFIG_S_DRIVER_OK);

  /* ----------  GET_DISPLAY_INFO  ---------- */
  struct virtio_gpu_ctrl_hdr cmd_get = {.type =
                                            VIRTIO_GPU_CMD_GET_DISPLAY_INFO};
  struct {
    struct virtio_gpu_ctrl_hdr hdr;
    struct {
      struct {
        uint32_t x, y, w, h;
      } r;
      uint32_t enabled;
      uint32_t flags;
    } pm[16];
  } resp_get;

  int head = send_cmd(&g->vdev, &g->q_ctrl, &cmd_get, sizeof(cmd_get),
                      &resp_get, sizeof(resp_get), 0);
  wait_cmd(&g->vdev, &g->q_ctrl, head);

  if (!resp_get.pm[0].enabled) {
    g->width = 1024;
    g->height = 768;
    print("virtio-gpu: display disabled; defaulting to 1024×768\n",
          PRINT_FLAG_BOTH);
  } else {
    g->width = resp_get.pm[0].r.w;
    g->height = resp_get.pm[0].r.h;
  }
  g->fb_bytes = g->width * g->height * 4;

  /* ----------  RESOURCE_CREATE_2D ---------- */
  static uint32_t next_res_id = 1;
  g->resource_id = next_res_id++;
  struct {
    struct virtio_gpu_ctrl_hdr hdr;
    uint32_t resource_id, format, width, height;
  } cmd_create = {.hdr = {.type = VIRTIO_GPU_CMD_RESOURCE_CREATE_2D},
                  .resource_id = g->resource_id,
                  .format = VIRTIO_GPU_FORMAT_B8G8R8A8_UNORM,
                  .width = g->width,
                  .height = g->height};
  head = send_cmd(&g->vdev, &g->q_ctrl, &cmd_create, sizeof(cmd_create), NULL,
                  0, 0);
  wait_cmd(&g->vdev, &g->q_ctrl, head);

  /* ----------  allocate & ATTACH_BACKING  ---------- */
  uint32_t pages = (g->fb_bytes + PAGE_SIZE - 1) / PAGE_SIZE;
  struct attach_entry {
    uint64_t addr;
    uint32_t len;
    uint32_t pad;
  };
  size_t attach_sz = sizeof(struct virtio_gpu_ctrl_hdr) + 8 + 4 +
                     pages * sizeof(struct attach_entry);
  void *attach_buf = alloc_page(); /* fits one page for ≤ 256 pages */
  memset(attach_buf, 0, attach_sz);

  struct virtio_gpu_ctrl_hdr *ahdr = attach_buf;
  ahdr->type = VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING;
  uint32_t *rid = (uint32_t *)(ahdr + 1);
  uint32_t *nr = rid + 1;
  *rid = g->resource_id;
  *nr = pages;

  struct attach_entry *ae = (struct attach_entry *)(nr + 1);
  g->fb_virt = alloc_page(); /* first page */
  for (uint32_t i = 0; i < pages; i++) {
    if (i) { /* extra pages */
      void *p = alloc_page();
      if (!p)
        panic("gpu: out of pages");
    }
    ae[i].addr = p2p((uint8_t *)g->fb_virt + i * PAGE_SIZE);
    ae[i].len = PAGE_SIZE;
  }
  memset(g->fb_virt, 0x00, g->fb_bytes); /* clear screen */

  head = send_cmd(&g->vdev, &g->q_ctrl, attach_buf, attach_sz, NULL, 0, 0);
  wait_cmd(&g->vdev, &g->q_ctrl, head);

  /* ----------  SET_SCANOUT  ---------- */
  struct {
    struct virtio_gpu_ctrl_hdr hdr;
    struct {
      uint32_t x, y, w, h;
    } r;
    uint32_t scanout, resource_id;
  } cmd_scan = {.hdr = {.type = VIRTIO_GPU_CMD_SET_SCANOUT},
                .resource_id = g->resource_id,
                .scanout = 0,
                .r = {0, 0, g->width, g->height}};
  head =
      send_cmd(&g->vdev, &g->q_ctrl, &cmd_scan, sizeof(cmd_scan), NULL, 0, 0);
  wait_cmd(&g->vdev, &g->q_ctrl, head);

  /* ----------  initial white screen + FLUSH  ---------- */
  uint32_t *px = g->fb_virt;
  for (uint64_t i = 0; i < g->width * (uint64_t)g->height; i++)
    px[i] = 0xFFFFFFFF;

  struct {
    struct virtio_gpu_ctrl_hdr hdr;
    struct {
      uint32_t x, y, w, h;
    } r;
    uint64_t offset;
    uint32_t resource_id;
    uint32_t pad;
  } cmd_xfer = {.hdr = {.type = VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D},
                .resource_id = g->resource_id,
                .r = {0, 0, g->width, g->height},
                .offset = 0};
  head =
      send_cmd(&g->vdev, &g->q_ctrl, &cmd_xfer, sizeof(cmd_xfer), NULL, 0, 0);
  wait_cmd(&g->vdev, &g->q_ctrl, head);

  struct {
    struct virtio_gpu_ctrl_hdr hdr;
    struct {
      uint32_t x, y, w, h;
    } r;
    uint32_t resource_id;
    uint32_t pad;
  } cmd_flush = {.hdr = {.type = VIRTIO_GPU_CMD_RESOURCE_FLUSH},
                 .resource_id = g->resource_id,
                 .r = {0, 0, g->width, g->height}};
  head =
      send_cmd(&g->vdev, &g->q_ctrl, &cmd_flush, sizeof(cmd_flush), NULL, 0, 0);
  wait_cmd(&g->vdev, &g->q_ctrl, head);

  printf("virtio-gpu: ready – %{type: uint}x%{type: uint} framebuffer mapped "
         "at %{type: ptr}\n",
         PRINT_FLAG_BOTH, g->width, g->height, g->fb_virt);
  return true;
}

/* ------------------------------------------------------------------ ISR */
void virtio_gpu_handle_irq(virtio_gpu_t *g) {
  if (!g)
    return;
  virtio_ack_irq(&g->vdev);

  /* process any completed commands (free chains) */
  while (g->q_ctrl.last_used_idx != g->q_ctrl.used->idx) {
    int id = g->q_ctrl.used->ring[g->q_ctrl.last_used_idx % g->q_ctrl.size].id;
    g->q_ctrl.last_used_idx++;
    free_chain(&g->q_ctrl, id);
  }
}
