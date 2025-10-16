#include "block.h"
#include "../core/queue.h"
#include "../core/mmio.h"
#include <lib/memory.h>
#include <lib/kalloc.h>
#include <physical_alloc.h>
#include <page_table.h>
#include <lib/print.h>
#include <stddef.h>
#include <device/virtio/virtio.h>

#define VIRTIO_BLK_T_IN   0
#define VIRTIO_BLK_T_OUT  1
#define VIRTIO_BLK_S_OK   0

static int blk_init(virtio_block_dev_t *blk) {
  // Negotiate minimal features: VERSION_1 mandatory
  uint64_t out;
  int rc = virtio_device_negotiate(blk->vdev, 0, VIRTIO_F_VERSION_1, &out);
  if (rc) return rc;

  // Read capacity
  virtio_blk_config_t *cfg = (virtio_blk_config_t *)(blk->vdev->mmio_base + VIRTIO_MMIO_CONFIG);
  blk->capacity = cfg->capacity;
  blk->sector_size = 512;

  // Create request queue 0
  rc = virtq_create(blk->vdev, 0, 8, &blk->rq);
  if (rc) return rc;

  // Driver OK
  uint32_t s = virtio_mmio_read32(blk->vdev->mmio_base, VIRTIO_MMIO_STATUS);
  virtio_mmio_write32(blk->vdev->mmio_base, VIRTIO_MMIO_STATUS, s | VIRTIO_CONFIG_S_DRIVER_OK);
#if VIRTIO_DEBUG
  printf("virtio-blk: capacity(sectors)=%{type: int}\n", PRINT_FLAG_BOTH, (int)blk->capacity);
#endif
  return 0;
}

int virtio_blk_probe(virtio_device_t *dev) {
  virtio_block_dev_t *blk = (virtio_block_dev_t *)kalloc(sizeof(virtio_block_dev_t));
  if (!blk) return -1;
  memset(blk, 0, sizeof(virtio_block_dev_t));
  blk->vdev = dev;
  int rc = blk_init(blk);
  if (rc) return rc;
  // Store globally for now so main() can retrieve it
  extern void virtio_blk_set_global(virtio_block_dev_t *);
  virtio_blk_set_global(blk);
  return 0;
}

typedef struct {
  virtio_blk_req_hdr_t hdr;
  uint8_t              status;
} blk_sync_tail_t;

static g_bool submit_rw(virtio_block_dev_t *blk, uint32_t type, uint64_t sector, void *buf, uint32_t num_sectors, g_bool is_write) {
  blk_sync_tail_t *tail = (blk_sync_tail_t *)kalloc(sizeof(blk_sync_tail_t));
  if (!tail) return false;
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
  printf("virtio-blk: %s sector=%{type: int} n=%{type: int} status=%{type: int}\n", PRINT_FLAG_BOTH,
         is_write ? "write" : "read", (int)sector, (int)num_sectors, (int)tail->status);
#endif
  return ok;
}

g_bool virtio_blk_read(virtio_block_dev_t *blk, uint64_t sector, void *buf, uint32_t num_sectors) {
  return submit_rw(blk, VIRTIO_BLK_T_IN, sector, buf, num_sectors, false);
}

g_bool virtio_blk_write(virtio_block_dev_t *blk, uint64_t sector, const void *buf, uint32_t num_sectors) {
  return submit_rw(blk, VIRTIO_BLK_T_OUT, sector, (void *)buf, num_sectors, true);
}

// Simple global slot (single device for now)
static virtio_block_dev_t *g_virtio_blk0;

virtio_block_dev_t *virtio_blk_get(void) { return g_virtio_blk0; }

void virtio_blk_set_global(virtio_block_dev_t *blk) { g_virtio_blk0 = blk; }



