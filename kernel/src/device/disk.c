#include "disk.h"
#include <physical_alloc.h>
#include <lib/print.h>

RESULT_TYPE(disk_t *) make_disk(virtio_block_t *vblk)
{
  disk_t *d = (disk_t *)alloc_page();
  if (!d)
    return RESULT_FAILURE(RESULT_NOMEM);
  d->vblk = vblk;
  d->sector_size = 512;
  d->capacity_sectors = 0;
  d->is_initialized = false;
  return RESULT_SUCCESS(d);
}

g_bool disk_init(disk_t *d)
{
  if (!d || !d->vblk)
    return false;
  if (!d->vblk->vdev.is_initialized)
    return false;
  d->sector_size = d->vblk->sector_size;
  d->capacity_sectors = d->vblk->capacity;
  d->is_initialized = true;
  return true;
}

g_bool disk_read(disk_t *d, uint64_t sector, void *buf, uint32_t num_sectors)
{
  if (!d || !d->is_initialized)
    return false;
  return virtio_block_read(d->vblk, sector, buf, num_sectors);
}

g_bool disk_write(disk_t *d, uint64_t sector, const void *buf, uint32_t num_sectors)
{
  if (!d || !d->is_initialized)
    return false;
  return virtio_block_write(d->vblk, sector, buf, num_sectors);
}


