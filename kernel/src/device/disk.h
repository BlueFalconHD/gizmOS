#pragma once

#include <lib/types.h>
#include <lib/result.h>
#include <device/virtio/drivers/block.h>

typedef struct {
  virtio_block_dev_t *vblk;
  uint32_t sector_size;
  uint64_t capacity_sectors;
  g_bool is_initialized;
} disk_t;

RESULT_TYPE(disk_t *) make_disk(virtio_block_dev_t *vblk);

g_bool disk_init(disk_t *d);

/* Synchronous sector IO using the underlying VirtIO block device */
g_bool disk_read(disk_t *d, uint64_t sector, void *buf, uint32_t num_sectors);
g_bool disk_write(disk_t *d, uint64_t sector, const void *buf, uint32_t num_sectors);

static inline uint64_t disk_capacity(const disk_t *d) { return d->capacity_sectors; }
static inline uint32_t disk_sector_size(const disk_t *d) { return d->sector_size; }


