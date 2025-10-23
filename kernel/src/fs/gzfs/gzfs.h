#pragma once

#include <fs/vfs.h>
#include <fs/bcache.h>

#define GZFS_MAGIC 0x3153465A47ULL /* 'GZFS1' little endian */

typedef struct __attribute__((packed)) {
  uint64_t magic;
  uint32_t block_size;      /* expect 4096 */
  uint32_t total_blocks;
  uint32_t inode_count;
  uint32_t journal_start;
  uint32_t journal_len;
  uint32_t inode_start;
  uint32_t inode_blocks;
  uint32_t bitmap_start;
  uint32_t bitmap_blocks;
  uint32_t data_start;
  uint32_t reserved;
} gzfs_super_t;

typedef struct {
  bcache_t *bc;
  gzfs_super_t sb;
} gzfs_fs_t;

typedef struct __attribute__((packed)) {
  uint16_t mode;
  uint16_t type; /* vfs_node_kind_t */
  uint32_t nlink;
  uint64_t size;
  uint64_t atime;
  uint64_t mtime;
  uint64_t ctime;
  uint32_t direct[12];
  uint32_t indirect1;
  uint32_t indirect2;
  uint32_t reserved[3];
  char symlink_inline[88];
} gzfs_inode_disk_t;

typedef struct {
  gzfs_fs_t *fs;
  uint32_t inum;
  gzfs_inode_disk_t din;
} gzfs_inode_t;

extern const vfs_fs_type_t gzfs_type;

/*
 * Super extended attributes / named forks support.
 *
 * We use inode.reserved[0] as a pointer to a fork table block (relative to
 * fs->sb.data_start). When zero, the inode has no forks.
 *
 * The fork table block contains a 8-byte header followed by fixed-size entries
 * describing each fork's data stream (block-mapped similarly to regular files).
 */

typedef struct __attribute__((packed)) {
  uint32_t count;     /* number of valid entries following */
  uint32_t _reserved; /* must be zero */
} gzfs_fork_table_header_disk_t;

typedef struct __attribute__((packed)) {
  uint8_t  namelen;   /* length of name (<= 63) */
  uint8_t  flags;     /* reserved for future */
  uint16_t _pad;      /* alignment */
  char     name[64];  /* UTF-8, not null-terminated if namelen==64 */
  uint64_t size;      /* bytes */
  uint32_t direct[12];
  uint32_t indirect1;
  uint32_t _reserved32;
} gzfs_fork_entry_disk_t;

/* Runtime-only private tags attached to vnodes to represent forks */
typedef enum {
  GZFS_PRIV_NONE = 0,
  GZFS_PRIV_FORKDIR = 1,
  GZFS_PRIV_FORKFILE = 2,
} gzfs_private_kind_t;

typedef struct {
  uint32_t kind;      /* gzfs_private_kind_t */
  uint32_t parent_inum;
} gzfs_forkdir_priv_t;

typedef struct {
  uint32_t kind;      /* gzfs_private_kind_t */
  uint32_t parent_inum;
  gzfs_fork_entry_disk_t entry; /* snapshot of on-disk entry */
} gzfs_fork_priv_t;


