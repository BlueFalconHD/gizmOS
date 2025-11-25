// ObjectFS public kernel interface (read-only MVP)
#pragma once

#include <lib/result.h>
#include <lib/types.h>
#include <device/disk.h>

#include "objfs_format.h"
#include "objfs_bcache.h"

typedef struct objfs_fs objfs_fs_t;

typedef struct {
  uint64_t size;
  uint16_t mode;
  uint8_t  kind;     // objfs_obj_kind_t
  uint32_t nlink;
} objfs_stat_t;

struct objfs_fs {
  objfs_bcache_t *bc;
  objfs_superblock_t sb;
  uint64_t blocks_total;
};

// No fake object identifiers; ObjectFS resolves only on-disk objects.

// Global filesystem instance (root mount)
objfs_fs_t *objfs_global(void);

// Mount ObjectFS as root using the provided disk
result_t objfs_mount_root(disk_t *disk);

// Resolve a path to an object id (handles references)
result_t objfs_lookup_path(const char *path, uint64_t *out_obj_id);

// Query object metadata
result_t objfs_object_stat(uint64_t obj_id, objfs_stat_t *out);

// Iterate subobjects of a directory object (emits name, kind, id)
typedef void (*objfs_emit_subobject_fn)(const char *name, uint8_t kind, uint64_t id,
                                        void *arg);
result_t objfs_list_subobjects(uint64_t dir_id, objfs_emit_subobject_fn emit, void *arg);

// Count subobjects in a directory
result_t objfs_subobjects_count(uint64_t dir_id, uint64_t *out_count);

// Get subobject at a given index (0-based). Optionally returns name/kind/id.
result_t objfs_subobject_at(uint64_t dir_id, uint64_t index,
                            char *name_buf, size_t name_cap,
                            uint8_t *out_kind, uint64_t *out_id);

// Read file contents
result_t objfs_read(uint64_t obj_id, uint64_t off, void *buf, size_t n, size_t *out);

// Write support (Phase 2)
result_t objfs_write_bytes(uint64_t obj_id, uint64_t off, const void *buf, size_t n, size_t *out);
result_t objfs_create(uint64_t parent_dir_id, const char *name, uint16_t mode, uint8_t kind, uint64_t *out_obj_id);
result_t objfs_link(uint64_t parent_dir_id, const char *name, uint64_t target_id);
result_t objfs_unlink(uint64_t parent_dir_id, const char *name);
result_t objfs_rename(uint64_t parent_dir_id, const char *old_name, const char *new_name);
result_t objfs_set_attr_str(uint64_t obj_id, const char *key, const char *value);
result_t objfs_set_attr_int(uint64_t obj_id, const char *key, int64_t value);
result_t objfs_set_attr_bool(uint64_t obj_id, const char *key, uint8_t value);

// Allocator helpers (phase 2)
result_t objfs_alloc_run(uint32_t need, uint64_t *out_first);
result_t objfs_free_run(uint64_t first, uint32_t count);

// Attributes (typed)
typedef enum {
  OBJFS_ATTR_V_STR = 0,
  OBJFS_ATTR_V_INT = 1,
  OBJFS_ATTR_V_BOOL = 2,
} objfs_attr_value_kind_t;

typedef struct {
  uint8_t type;  // objfs_attr_value_kind_t
  union {
    int64_t i64;
    uint8_t b;
  } v;
  // For strings, we copy out into caller's buffer via objfs_attr_get_str
} objfs_attr_value_t;

result_t objfs_attr_get(uint64_t obj_id, const char *key, objfs_attr_value_t *out);
result_t objfs_attr_get_str(uint64_t obj_id, const char *key, char *buf, size_t cap, size_t *out_len);
typedef void (*objfs_emit_attr_fn)(const char *key, uint8_t type, void *arg);
result_t objfs_list_attrs(uint64_t obj_id, objfs_emit_attr_fn emit, void *arg);

// Expose descriptor snapshot for userspace consumption via syscall
result_t objfs_read_descriptor(uint64_t obj_id, objfs_object_disk_t *out);

