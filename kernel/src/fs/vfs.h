#pragma once

#include <device/disk.h>
#include <lib/result.h>
#include <lib/types.h>
#include <lib/spinlock.h>

typedef struct vfs_node vfs_node_t;
typedef struct vfs_mount vfs_mount_t;
typedef struct vfs_fs_type vfs_fs_type_t;

typedef enum {
  VFS_NODE_UNKNOWN = 0,
  VFS_NODE_FILE = 1,
  VFS_NODE_DIR = 2,
  VFS_NODE_SYMLINK = 3,
} vfs_node_kind_t;

typedef struct vfs_stat {
  uint64_t size;
  uint16_t mode;
  uint16_t type; /* vfs_node_kind_t */
  uint32_t nlink;
} vfs_stat_t;

typedef struct vfs_ops {
  result_t (*lookup)(vfs_mount_t *mnt, vfs_node_t *dir, const char *name,
                     vfs_node_t **out);
  result_t (*getattr)(vfs_mount_t *mnt, vfs_node_t *node, vfs_stat_t *st);
  result_t (*read)(vfs_mount_t *mnt, vfs_node_t *node, uint64_t off, void *buf,
                   size_t n, size_t *out);
  result_t (*readdir)(vfs_mount_t *mnt, vfs_node_t *dir,
                      void (*emit)(const char *name, uint32_t type, void *arg),
                      void *arg);
  /* Optional, may return RESULT_NOTIMPL */
  result_t (*create)(vfs_mount_t *mnt, vfs_node_t *dir, const char *name,
                     uint16_t mode, vfs_node_t **out);
  result_t (*mkdir)(vfs_mount_t *mnt, vfs_node_t *dir, const char *name,
                    uint16_t mode);
  result_t (*link)(vfs_mount_t *mnt, vfs_node_t *dir, const char *name,
                   vfs_node_t *target);
  result_t (*unlink)(vfs_mount_t *mnt, vfs_node_t *dir, const char *name);
  result_t (*symlink)(vfs_mount_t *mnt, vfs_node_t *dir, const char *name,
                      const char *target);
  result_t (*rename)(vfs_mount_t *mnt, vfs_node_t *odir, const char *oname,
                     vfs_mount_t *nmnt, vfs_node_t *ndir, const char *nname);
  result_t (*setattr)(vfs_mount_t *mnt, vfs_node_t *node,
                      const vfs_stat_t *st);
} vfs_ops_t;

struct vfs_node {
  vfs_mount_t *mnt;
  uint32_t inum;            /* filesystem-local inode number */
  uint16_t mode;            /* mode bits, fs-defined */
  uint8_t  type;            /* vfs_node_kind_t */
  void *fs_private;         /* fs-private pointer (may be NULL) */
};

struct vfs_mount {
  disk_t *disk;             /* underlying block device */
  const vfs_fs_type_t *type;
  const vfs_ops_t *ops;
  vfs_node_t *root;         /* root directory */
  void *fs_private;         /* fs instance data */
};

struct vfs_fs_type {
  const char *name;
  /* Create a mount. Must set ops, root and fs_private. */
  result_t (*mount)(disk_t *disk, vfs_mount_t **out);
};

void vfs_init(void);
RESULT_TYPE(void) vfs_set_root(vfs_mount_t *mnt);
result_t vfs_mount_root(disk_t *disk, const vfs_fs_type_t *type);
result_t vfs_lookup(const char *path, vfs_node_t **out);
result_t vfs_getattr(const char *path, vfs_stat_t *st);
result_t vfs_read_entire(const char *path, void *buf, size_t cap, size_t *n);

/* Accessors */
vfs_mount_t *vfs_root_mount(void);
vfs_node_t *vfs_root_node(void);


