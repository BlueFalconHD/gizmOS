#include "objfs.h"
#include <lib/kalloc.h>
#include <lib/memory.h>
#include <lib/str.h>

static objfs_fs_t *g_objfs = NULL;

objfs_fs_t *objfs_global(void) {
  return g_objfs;
}

static g_bool magic_ok(const char magic[8]) {
  const char *m = OBJFS_MAGIC_STR;
  for (int i = 0; i < OBJFS_MAGIC_LEN; i++) {
    if (magic[i] != m[i])
      return false;
  }
  return true;
}

result_t objfs_mount_root(disk_t *disk) {
  if (!disk)
    return RESULT_FAILURE(RESULT_INVALID);

  // Read block 0 (superblock)
  uint8_t *blk = (uint8_t *)kalloc(OBJFS_BLOCK_SIZE_DEFAULT);
  if (!blk)
    return RESULT_FAILURE(RESULT_NOMEM);

  // Assume superblock uses default block size for initial read
  uint32_t sector_size = disk_sector_size(disk);
  if (sector_size == 0 || (OBJFS_BLOCK_SIZE_DEFAULT % sector_size) != 0) {
    kfree(blk);
    return RESULT_FAILURE(RESULT_INVALID);
  }
  uint32_t nsectors = OBJFS_BLOCK_SIZE_DEFAULT / sector_size;
  if (!disk_read(disk, 0, blk, nsectors)) {
    kfree(blk);
    return RESULT_FAILURE(RESULT_ERROR);
  }

  objfs_superblock_t sb_tmp;
  memcpy(&sb_tmp, blk, sizeof(sb_tmp));
  kfree(blk);

  if (!magic_ok(sb_tmp.magic))
    return RESULT_FAILURE(RESULT_ERROR);
  if (sb_tmp.block_size == 0)
    return RESULT_FAILURE(RESULT_INVALID);

  // Create bc and fs
  RESULT_TYPE(objfs_bcache_t *) rbc = objfs_bcache_create(disk, sb_tmp.block_size);
  if (!result_is_ok(rbc))
    return rbc;
  objfs_bcache_t *bc = (objfs_bcache_t *)result_unwrap(rbc);

  objfs_fs_t *fs = (objfs_fs_t *)kalloc(sizeof(objfs_fs_t));
  if (!fs) {
    objfs_bcache_destroy(bc);
    return RESULT_FAILURE(RESULT_NOMEM);
  }
  fs->bc = bc;
  fs->sb = sb_tmp;
  // compute total blocks
  uint64_t total_sectors = disk_capacity(disk);
  uint64_t bytes_total = total_sectors * (uint64_t)bc->sector_size;
  fs->blocks_total = bytes_total / (uint64_t)sb_tmp.block_size;

  g_objfs = fs;
  // Register built-in virtual providers (e.g., Devices) after mount.
  extern void objfs_virtual_init_after_mount(void);
  objfs_virtual_init_after_mount();
  return RESULT_SUCCESS(0);
}


