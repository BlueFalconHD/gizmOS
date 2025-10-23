#include "gzfs.h"
#include <lib/kalloc.h>
#include <lib/memory.h>
#include <lib/print.h>

static result_t gzfs_load_super(disk_t *disk, gzfs_super_t *out) {
  uint8_t buf[BCACHE_BLOCK_SIZE];
  /* super at block 0 */
  if (!disk_read(disk, 0, buf, (BCACHE_BLOCK_SIZE / 512)))
    return RESULT_FAILURE(RESULT_ERROR);
  memcpy(out, buf, sizeof(gzfs_super_t));
  if (out->magic != GZFS_MAGIC) return RESULT_FAILURE(RESULT_NOENT);
  if (out->block_size != BCACHE_BLOCK_SIZE) return RESULT_FAILURE(RESULT_INVALID);
  return RESULT_SUCCESS(0);
}

static result_t gzfs_mount_impl(disk_t *disk, vfs_mount_t **out_mnt) {
  gzfs_super_t sb;
  result_t r = gzfs_load_super(disk, &sb);
  if (!result_is_ok(r)) return r;

  gzfs_fs_t *fs = (gzfs_fs_t *)kalloc(sizeof(gzfs_fs_t));
  if (!fs) return RESULT_FAILURE(RESULT_NOMEM);
  memset(fs, 0, sizeof(*fs));
  fs->sb = sb;
  fs->bc = bcache_create(disk, 128);
  if (!fs->bc) { kfree(fs); return RESULT_FAILURE(RESULT_NOMEM); }

  vfs_mount_t *mnt = (vfs_mount_t *)kalloc(sizeof(vfs_mount_t));
  if (!mnt) { bcache_destroy(fs->bc); kfree(fs); return RESULT_FAILURE(RESULT_NOMEM); }
  memset(mnt, 0, sizeof(*mnt));
  mnt->disk = disk;
  mnt->fs_private = fs;

  /* fill ops later from gzfs_ops defined in other module */
  extern const vfs_ops_t gzfs_ops;
  mnt->ops = &gzfs_ops;

  /* root inode is 1 */
  vfs_node_t *root = (vfs_node_t *)kalloc(sizeof(vfs_node_t));
  if (!root) { kfree(mnt); bcache_destroy(fs->bc); kfree(fs); return RESULT_FAILURE(RESULT_NOMEM); }
  memset(root, 0, sizeof(*root));
  root->mnt = mnt;
  root->inum = 1;
  root->type = VFS_NODE_DIR;
  root->mode = 0755;
  mnt->root = root;

  *out_mnt = mnt;
  return RESULT_SUCCESS(0);
}

const vfs_fs_type_t gzfs_type = {
  .name = "gzfs",
  .mount = gzfs_mount_impl,
};


