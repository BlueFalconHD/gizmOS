#include "virtio_block.h"
#include "virtio_bus.h"
#include <lib/memory.h>
#include <lib/panic.h>
#include <lib/print.h>
#include <physical_alloc.h>
#include <page_table.h>

#define VIRTIO_BLK_QUEUE_SIZE  8

RESULT_TYPE(virtio_block_t *)
make_virtio_block(uint64_t base, uint32_t irq)
{
  virtio_block_t *blk = (virtio_block_t *)alloc_page();
  if (!blk)
    return RESULT_FAILURE(RESULT_NOMEM);

  blk->vdev.base = base;
  blk->vdev.irq = irq;
  blk->vdev.is_initialized = false;
  blk->vdev.driver_data = blk;
  blk->vdev.isr = (void (*)(void *))virtio_block_handle_irq;
  blk->sector_size = 512;
  blk->capacity = 0;
  blk->status = 0xff;
  memset(&blk->req_hdr, 0, sizeof(blk->req_hdr));

  return RESULT_SUCCESS(blk);
}

static inline struct virtio_blk_config *blk_cfg(virtio_block_t *blk)
{
  return (struct virtio_blk_config *)(blk->vdev.base + VIRTIO_MMIO_CONFIG);
}

g_bool virtio_block_init(virtio_block_t *blk)
{
  if (!blk)
    return false;

  if (!virtio_device_init(&blk->vdev, 0))
    return false;

  if (blk->vdev.device_id != VIRTIO_DEV_BLOCK) {
    print("virtio-blk: wrong device id\n", PRINT_FLAG_BOTH);
    return false;
  }

  /* read device capacity in sectors */
  blk->capacity = blk_cfg(blk)->capacity;

  /* single request queue */
  if (!virtio_queue_setup_empty(&blk->vdev, &blk->q, /*qsel*/0,
                                VIRTIO_BLK_QUEUE_SIZE)) {
    return false;
  }

  virtio_bus_register(&blk->vdev);
  virtio_set_driver_ok(&blk->vdev);
  return true;
}

/* Build a 3-descriptor chain: [hdr][data][status] */
static g_bool submit_rw(virtio_block_t *blk, uint32_t type, uint64_t sector,
                        void *buf, uint32_t num_sectors, g_bool is_write)
{
  uint32_t bytes = num_sectors * blk->sector_size;

  /* find 3 free descriptors (simplified: assume first three) */
  uint16_t d0 = 0, d1 = 1, d2 = 2;

  /* header */
  blk->req_hdr.type = type;
  blk->req_hdr.reserved = 0;
  blk->req_hdr.sector = sector;

  blk->q.desc[d0].addr = V2P((uint64_t)&blk->req_hdr);
  blk->q.desc[d0].len  = sizeof(blk->req_hdr);
  blk->q.desc[d0].flags = VRING_DESC_F_NEXT;
  blk->q.desc[d0].next  = d1;

  /* data */
  blk->q.desc[d1].addr = V2P((uint64_t)buf);
  blk->q.desc[d1].len  = bytes;
  blk->q.desc[d1].flags = (is_write ? 0 : VRING_DESC_F_WRITE) | VRING_DESC_F_NEXT;
  blk->q.desc[d1].next  = d2;

  /* status byte (device writes one byte) */
  blk->status = 0xff;
  blk->q.desc[d2].addr = V2P((uint64_t)&blk->status);
  blk->q.desc[d2].len  = 1;
  blk->q.desc[d2].flags = VRING_DESC_F_WRITE;

  /* submit chain */
  uint16_t head = d0;
  blk->q.avail->ring[blk->q.avail->idx % blk->q.size] = head;
  __sync_synchronize();
  blk->q.avail->idx++;
  virtio_queue_notify(&blk->vdev, 0);

  /* busy-wait for completion (simple first cut) */
  while (blk->q.last_used_idx == blk->q.used->idx) {
    /* spin */
  }
  /* advance local used pointer */
  blk->q.last_used_idx++;

  return blk->status == VIRTIO_BLK_S_OK;
}

g_bool virtio_block_read(virtio_block_t *blk, uint64_t sector,
                         void *buf, uint32_t num_sectors)
{
  return submit_rw(blk, VIRTIO_BLK_T_IN, sector, buf, num_sectors, /*is_write*/false);
}

g_bool virtio_block_write(virtio_block_t *blk, uint64_t sector,
                          const void *buf, uint32_t num_sectors)
{
  return submit_rw(blk, VIRTIO_BLK_T_OUT, sector, (void *)buf, num_sectors, /*is_write*/true);
}

void virtio_block_handle_irq(virtio_block_t *blk)
{
  (void)blk; /* we spin-wait in submit_rw; nothing to do here yet */
}


