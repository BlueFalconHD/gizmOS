#pragma once

#include "virtio_common.h"
#include <lib/types.h>
#include <lib/result.h>

/* VirtIO block (spec §5.2) */

/* Feature bits we may want later; for now keep 0 */
#define VIRTIO_BLK_F_RO         (1ULL << 5)
#define VIRTIO_BLK_F_BLK_SIZE   (1ULL << 6)
#define VIRTIO_BLK_F_FLUSH      (1ULL << 9)

/* Request types */
#define VIRTIO_BLK_T_IN     0
#define VIRTIO_BLK_T_OUT    1
#define VIRTIO_BLK_T_FLUSH  4

/* Status codes (device → driver) */
#define VIRTIO_BLK_S_OK     0
#define VIRTIO_BLK_S_IOERR  1
#define VIRTIO_BLK_S_UNSUPP 2

/* Per-spec request header placed in descriptor 0 */
struct virtio_blk_req_hdr {
  uint32_t type;      /* VIRTIO_BLK_T_* */
  uint32_t reserved;  /* 0 */
  uint64_t sector;    /* sector number */
} __attribute__((packed));

/* MMIO configuration space (base + VIRTIO_MMIO_CONFIG) */
struct virtio_blk_config {
  uint64_t capacity;  /* number of 512-byte sectors */
  // optional fields ignored for now
} __attribute__((packed));

typedef struct {
  virtio_device_t vdev;

  virtio_queue_t  q;              /* single request queue */

  /* Pre-allocated I/O structures for a simple synchronous API */
  struct virtio_blk_req_hdr req_hdr;
  uint8_t                   status;  /* completion status byte */

  uint32_t sector_size;     /* usually 512 */
  uint64_t capacity;        /* sectors */
} virtio_block_t;

RESULT_TYPE(virtio_block_t *) make_virtio_block(uint64_t base, uint32_t irq);
g_bool virtio_block_init(virtio_block_t *blk);

/* Synchronous read/write of n sectors starting at sector into buffer */
g_bool virtio_block_read(virtio_block_t *blk, uint64_t sector,
                         void *buf, uint32_t num_sectors);
g_bool virtio_block_write(virtio_block_t *blk, uint64_t sector,
                          const void *buf, uint32_t num_sectors);

/* ISR for bus callback */
void virtio_block_handle_irq(virtio_block_t *blk);


