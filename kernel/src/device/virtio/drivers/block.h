#pragma once

#include "../core/device.h"
#include "../core/spec.h"
#include <lib/spinlock.h>
#include <lib/types.h>
#include <stdint.h>

typedef struct virtio_blk_config {
  uint64_t capacity; // in 512-byte sectors
} __attribute__((packed)) virtio_blk_config_t;

typedef struct virtio_blk_req_hdr {
  uint32_t type; // 0=in,1=out,4=flush
  uint32_t reserved;
  uint64_t sector;
} __attribute__((packed)) virtio_blk_req_hdr_t;

typedef struct virtio_block_dev {
  virtio_device_t *vdev;
  struct virtq *rq;
  uint32_t sector_size;
  uint64_t capacity;    // sectors
  struct spinlock lock; // serialize submit_rw (single outstanding req)
} virtio_block_dev_t;

int virtio_blk_probe(virtio_device_t *dev);
g_bool virtio_blk_read(virtio_block_dev_t *blk, uint64_t sector, void *buf,
                       uint32_t num_sectors);
g_bool virtio_blk_write(virtio_block_dev_t *blk, uint64_t sector,
                        const void *buf, uint32_t num_sectors);

virtio_block_dev_t *virtio_blk_get(void);
