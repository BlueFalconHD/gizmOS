#include "block.h"
#include "../core/mmio.h"
#include "../core/queue.h"
#include <device/virtio/virtio.h>
#include <lib/kalloc.h>
#include <lib/log.h>
#include <lib/memory.h>
#include <lib/print.h>
#include <lib/spinlock.h>

static inline log_t *virtio_blk_log() {
  static log_t *l = NULL;
  if (!l) {
    l = g_log_create("virtio", "block");
#if VIRTIO_DEBUG
    g_log_set_level(l, LOG_LEVEL_DEBUG);
#else
    g_log_set_level(l, LOG_LEVEL_INFO);
#endif
  }
  return l;
}

#include <page_table.h>
#include <stddef.h>

#define VIRTIO_BLK_T_IN 0
#define VIRTIO_BLK_T_OUT 1
#define VIRTIO_BLK_S_OK 0

static int blk_init(virtio_block_dev_t *blk) {
  // Negotiate minimal features: VERSION_1 mandatory
  uint64_t out;
  int rc = virtio_device_negotiate(blk->vdev, 0, VIRTIO_F_VERSION_1, &out);
  if (rc)
    return rc;

  // Read capacity
  virtio_blk_config_t *cfg =
      (virtio_blk_config_t *)(blk->vdev->mmio_base + VIRTIO_MMIO_CONFIG);
  blk->capacity = cfg->capacity;
  blk->sector_size = 512;

  // Create request queue 0
  rc = virtq_create(blk->vdev, 0, 8, &blk->rq);
  if (rc)
    return rc;

  // Initialize per-device lock to serialize synchronous requests
  initlock(&blk->lock, "virtio_blk");

  // Driver OK
  uint32_t s = virtio_mmio_read32(blk->vdev->mmio_base, VIRTIO_MMIO_STATUS);
  virtio_mmio_write32(blk->vdev->mmio_base, VIRTIO_MMIO_STATUS,
                      s | VIRTIO_CONFIG_S_DRIVER_OK);
#if VIRTIO_DEBUG
  LOG_INFO(virtio_blk_log(), "capacity(sectors)=%{type: int}",
           (int)blk->capacity);
#endif
  return 0;
}

int virtio_blk_probe(virtio_device_t *dev) {
  virtio_block_dev_t *blk =
      (virtio_block_dev_t *)kalloc(sizeof(virtio_block_dev_t));
  if (!blk)
    return -1;
  memset(blk, 0, sizeof(virtio_block_dev_t));
  blk->vdev = dev;
  int rc = blk_init(blk);
  if (rc)
    return rc;
  // Store globally for now so main() can retrieve it
  extern void virtio_blk_set_global(virtio_block_dev_t *);
  virtio_blk_set_global(blk);
  return 0;
}

typedef struct {
  virtio_blk_req_hdr_t hdr;
  uint8_t status;
} blk_sync_tail_t;

static g_bool submit_rw(virtio_block_dev_t *blk, uint32_t type, uint64_t sector,
                        void *buf, uint32_t num_sectors, g_bool is_write) {
  acquire(&blk->lock);

  blk_sync_tail_t *tail = (blk_sync_tail_t *)kalloc(sizeof(blk_sync_tail_t));
  if (!tail) {
    release(&blk->lock);
    return false;
  }
  tail->hdr.type = type;
  tail->hdr.reserved = 0;
  tail->hdr.sector = sector;
  tail->status = 0xFF;

  struct iovec out_sg[2];
  struct iovec in_sg[2];
  size_t out_cnt = 1;
  size_t in_cnt = 1;
  out_sg[0].iov_base = &tail->hdr;
  out_sg[0].iov_len = sizeof(tail->hdr);

  if (is_write) {
    out_sg[1].iov_base = buf; // device reads data
    out_sg[1].iov_len = (size_t)num_sectors * blk->sector_size;
    out_cnt = 2;
    in_sg[0].iov_base = &tail->status; // device writes status
    in_sg[0].iov_len = 1;
    in_cnt = 1;
  } else {
    in_sg[0].iov_base = buf; // device writes data
    in_sg[0].iov_len = (size_t)num_sectors * blk->sector_size;
    in_sg[1].iov_base = &tail->status; // then writes status
    in_sg[1].iov_len = 1;
    in_cnt = 2;
  }

  virtq_submit(blk->rq, out_sg, out_cnt, in_sg, in_cnt, (void *)(uintptr_t)0);
  virtq_kick(blk->vdev, 0);

  // Busy wait simple path
  void *cookie;
  size_t bytes;
  while (virtq_poll_used(blk->rq, &cookie, &bytes) == 1) {
    // spin
  }
  g_bool ok = (tail->status == VIRTIO_BLK_S_OK);
#if VIRTIO_DEBUG
  LOG_DEBUG(
      virtio_blk_log(),
      "%{type: str} sector=%{type: int} n=%{type: int} status=%{type: int}",
      is_write ? "write" : "read", (int)sector, (int)num_sectors,
      (int)tail->status);
#endif
  kfree(tail);
  release(&blk->lock);
  return ok;
}

g_bool virtio_blk_read(virtio_block_dev_t *blk, uint64_t sector, void *buf,
                       uint32_t num_sectors) {
  return submit_rw(blk, VIRTIO_BLK_T_IN, sector, buf, num_sectors, false);
}

g_bool virtio_blk_write(virtio_block_dev_t *blk, uint64_t sector,
                        const void *buf, uint32_t num_sectors) {
  return submit_rw(blk, VIRTIO_BLK_T_OUT, sector, (void *)buf, num_sectors,
                   true);
}

// Simple global slot (single device for now)
static virtio_block_dev_t *g_virtio_blk0;

virtio_block_dev_t *virtio_blk_get(void) { return g_virtio_blk0; }

void virtio_blk_set_global(virtio_block_dev_t *blk) { g_virtio_blk0 = blk; }
