// Virtual ObjectFS providers (synthetic objects resolved programmatically)
#pragma once

#include "objfs.h"

typedef struct objfs_vops {
  // Resolve a path relative to this mount's root into a local object id.
  // relpath: "" for mount root, otherwise "name" or "name/..." segments.
  result_t (*lookup)(const char *relpath, uint64_t *out_local_id);
  // List subobjects of a local object id (emit name and synthesized child id).
  result_t (*list)(uint64_t local_id, objfs_emit_subobject_fn emit, void *arg);
  // Content IO
  result_t (*read)(uint64_t local_id, uint64_t off, void *buf, size_t n, size_t *out);
  result_t (*write)(uint64_t local_id, uint64_t off, const void *buf, size_t n, size_t *out);
  // Metadata
  result_t (*stat)(uint64_t local_id, objfs_stat_t *out);
  result_t (*desc)(uint64_t local_id, objfs_object_disk_t *out);
  // Attributes (optional)
  result_t (*attr_get)(uint64_t local_id, const char *key, objfs_attr_value_t *out);
  result_t (*attr_list)(uint64_t local_id, objfs_emit_attr_fn emit, void *arg);
} objfs_vops_t;

// Register a virtual mount at a directory under parent_id with given name.
// Returns the synthesized global root id for the mount.
result_t objfs_vreg_register(const char *mount_name, uint64_t parent_id,
                             const objfs_vops_t *ops, uint64_t *out_mount_root_id);

// Try to resolve an absolute path via registered virtual mounts.
result_t objfs_vreg_lookup_path(const char *path, uint64_t *out_obj_id);

// If id is virtual, return provider ops and local id for provider callbacks.
g_bool objfs_vreg_resolve(uint64_t global_id, const objfs_vops_t **out_ops, uint64_t *out_local_id);

// Emit mount points that are children of the given parent id.
result_t objfs_vreg_list_mounts(uint64_t parent_id, objfs_emit_subobject_fn emit, void *arg);

// Initialize built-in virtual providers after mount (e.g., Devices).
void objfs_virtual_init_after_mount(void);


