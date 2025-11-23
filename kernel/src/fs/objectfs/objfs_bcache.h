// Minimal block IO wrapper for ObjectFS (no caching in MVP)
#pragma once

#include <device/disk.h>
#include <lib/result.h>
#include <lib/types.h>
#include <lib/kalloc.h>

typedef struct {
  disk_t   *disk;
  uint32_t  block_size;   // bytes per block
  uint32_t  sector_size;  // disk sector size in bytes
} objfs_bcache_t;

static inline RESULT_TYPE(objfs_bcache_t *)
objfs_bcache_create(disk_t *disk, uint32_t block_size) {
  if (!disk || block_size == 0)
    return RESULT_FAILURE(RESULT_INVALID);
  objfs_bcache_t *bc = (objfs_bcache_t *)kalloc(sizeof(objfs_bcache_t));
  if (!bc)
    return RESULT_FAILURE(RESULT_NOMEM);
  bc->disk = disk;
  bc->block_size = block_size;
  bc->sector_size = disk_sector_size(disk);
  if (bc->sector_size == 0 || (block_size % bc->sector_size) != 0) {
    kfree(bc);
    return RESULT_FAILURE(RESULT_INVALID);
  }
  return RESULT_SUCCESS(bc);
}

static inline void objfs_bcache_destroy(objfs_bcache_t *bc) {
  if (bc)
    kfree(bc);
}

// Read a whole block (block_index relative to disk start)
static inline result_t objfs_block_read(objfs_bcache_t *bc, uint64_t block_index,
                                        void *out_buf) {
  if (!bc || !out_buf)
    return RESULT_FAILURE(RESULT_INVALID);
  uint64_t first_sector =
      (block_index * (uint64_t)bc->block_size) / (uint64_t)bc->sector_size;
  uint32_t nsectors = bc->block_size / bc->sector_size;
  return disk_read(bc->disk, first_sector, out_buf, nsectors)
             ? RESULT_SUCCESS(0)
             : RESULT_FAILURE(RESULT_ERROR);
}

// Write a whole block (block_index relative to disk start)
static inline result_t objfs_block_write(objfs_bcache_t *bc, uint64_t block_index,
                                         const void *in_buf) {
  if (!bc || !in_buf)
    return RESULT_FAILURE(RESULT_INVALID);
  uint64_t first_sector =
      (block_index * (uint64_t)bc->block_size) / (uint64_t)bc->sector_size;
  uint32_t nsectors = bc->block_size / bc->sector_size;
  return disk_write(bc->disk, first_sector, in_buf, nsectors)
             ? RESULT_SUCCESS(0)
             : RESULT_FAILURE(RESULT_ERROR);
}


