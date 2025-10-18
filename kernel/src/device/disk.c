#include "disk.h"
#include <lib/kalloc.h>
#include <lib/print.h>

#define DISK_DRIVER_DEBUG 1

RESULT_TYPE(disk_t *) make_disk(virtio_block_dev_t *vblk) {
  disk_t *d = (disk_t *)kalloc(sizeof(disk_t));
  if (!d)
    return RESULT_FAILURE(RESULT_NOMEM);
  d->vblk = vblk;
  d->sector_size = 512;
  d->capacity_sectors = 0;
  d->is_initialized = false;
  return RESULT_SUCCESS(d);
}

g_bool disk_init(disk_t *d) {
  if (!d || !d->vblk) {
#if DISK_DRIVER_DEBUG
    printf("disk_init: failed to create disk. d: %{type: ptr}, d->vblk: "
           "%{type: ptr}\n",
           PRINT_FLAG_BOTH, d, d->vblk);
#endif
    return false;
  }
  if (!d->vblk->vdev || !d->vblk->vdev->is_initialized) {
#if DISK_DRIVER_DEBUG
    printf("disk_init: failed to initialize disk. d->vblk->vdev: %{type: ptr}, "
           "d->vblk->vdev->is_initialized: %{type: bool}\n",
           PRINT_FLAG_BOTH, d->vblk->vdev, d->vblk->vdev->is_initialized);
#endif
  }
  d->sector_size = d->vblk->sector_size;
  d->capacity_sectors = d->vblk->capacity;
  d->is_initialized = true;
  return true;
}

g_bool disk_read(disk_t *d, uint64_t sector, void *buf, uint32_t num_sectors) {
  if (!d || !d->is_initialized)
    return false;
  return virtio_blk_read(d->vblk, sector, buf, num_sectors);
}

g_bool disk_write(disk_t *d, uint64_t sector, const void *buf,
                  uint32_t num_sectors) {
  if (!d || !d->is_initialized)
    return false;
  return virtio_blk_write(d->vblk, sector, buf, num_sectors);
}
