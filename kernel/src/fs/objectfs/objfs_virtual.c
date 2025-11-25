#include "objfs_virtual.h"
#include <lib/kalloc.h>
#include <lib/memory.h>
#include <lib/str.h>
#include <lib/print.h>

typedef struct {
  char name[64];
  uint64_t parent_id;
  const objfs_vops_t *ops;
  uint16_t index; // mount index
} vmount_t;

#define MAX_VMOUNTS 8
static vmount_t g_vmounts[MAX_VMOUNTS];
static uint16_t g_vmount_count = 0;

// Global id encoding:
// [15:0]=0xFFFF tag, [31:16]=mount index, [63:32]=local id high, [31:0]=local id low
static inline uint64_t make_global_id(uint16_t index, uint64_t local_id) {
  return (0xFFFFull) | ((uint64_t)index << 16) | (local_id << 32);
}
static inline g_bool is_global_virtual(uint64_t gid) {
  return (gid & 0xFFFFull) == 0xFFFFull;
}
static inline uint16_t global_index(uint64_t gid) {
  return (uint16_t)((gid >> 16) & 0xFFFFull);
}
static inline uint64_t global_local(uint64_t gid) {
  return (gid >> 32);
}

result_t objfs_vreg_register(const char *mount_name, uint64_t parent_id,
                             const objfs_vops_t *ops, uint64_t *out_mount_root_id) {
  if (!mount_name || !ops) return RESULT_FAILURE(RESULT_INVALID);
  if (g_vmount_count >= MAX_VMOUNTS) return RESULT_FAILURE(RESULT_BUSY);
  uint16_t idx = (uint16_t)g_vmount_count;
  vmount_t *m = &g_vmounts[g_vmount_count++];
  size_t n = strlen(mount_name); if (n > 63) n = 63;
  for (size_t i = 0; i < 64; i++) m->name[i] = (i < n) ? mount_name[i] : '\0';
  m->parent_id = parent_id;
  m->ops = ops;
  m->index = idx;
  uint64_t root_gid = make_global_id(m->index, 1); // local root = 1
  if (out_mount_root_id) *out_mount_root_id = root_gid;
  return RESULT_SUCCESS(0);
}

g_bool objfs_vreg_resolve(uint64_t global_id, const objfs_vops_t **out_ops, uint64_t *out_local_id) {
  if (!is_global_virtual(global_id)) return false;
  uint16_t idx = global_index(global_id);
  if (idx >= g_vmount_count) return false;
  vmount_t *m = &g_vmounts[idx];
  if (out_ops) *out_ops = m->ops;
  if (out_local_id) *out_local_id = global_local(global_id);
  return true;
}

result_t objfs_vreg_lookup_path(const char *path, uint64_t *out_obj_id) {
  if (!path || !out_obj_id) return RESULT_FAILURE(RESULT_INVALID);
  if (g_vmount_count == 0) return RESULT_FAILURE(RESULT_NOT_FOUND);
  // Split first component
  const char *p = path;
  while (*p == '/') p++;
  char first[65];
  size_t i = 0;
  while (p[i] && p[i] != '/' && i < 64) { first[i] = p[i]; i++; }
  first[i] = '\0';
  const char *rest = p + i;
  for (uint16_t k = 0; k < g_vmount_count; k++) {
    vmount_t *m = &g_vmounts[k];
    if (strcmp(first, m->name) == 0) {
      while (*rest == '/') rest++;
      uint64_t local = 0;
      if (!m->ops->lookup) return RESULT_FAILURE(RESULT_NOT_FOUND);
      result_t rl = m->ops->lookup(rest, &local);
      if (!result_is_ok(rl)) return rl;
      *out_obj_id = make_global_id(m->index, local);
      return RESULT_SUCCESS(0);
    }
  }
  return RESULT_FAILURE(RESULT_NOT_FOUND);
}

result_t objfs_vreg_list_mounts(uint64_t parent_id, objfs_emit_subobject_fn emit, void *arg) {
  if (!emit) return RESULT_FAILURE(RESULT_INVALID);
  for (uint16_t k = 0; k < g_vmount_count; k++) {
    vmount_t *m = &g_vmounts[k];
    if (m->parent_id == parent_id) {
      uint64_t gid = make_global_id(m->index, 1);
      emit(m->name, OBJFS_OBJ_DIR, gid, arg);
    }
  }
  return RESULT_SUCCESS(0);
}

// Built-in Devices provider
static uint16_t g_dev_mount_index = 0;
static result_t dev_lookup(const char *rel, uint64_t *out_local) {
  if (!rel || !*rel) { *out_local = 1; return RESULT_SUCCESS(0); } // root
  // take first component
  const char *p = rel;
  char name[65]; size_t n = 0;
  while (p[n] && p[n] != '/' && n < 64) { name[n] = p[n]; n++; }
  name[n] = '\0';
  if (strcmp(name, "Console.out") == 0) { *out_local = 2; return RESULT_SUCCESS(0); }
  if (strcmp(name, "Uart.out") == 0) { *out_local = 3; return RESULT_SUCCESS(0); }
  return RESULT_FAILURE(RESULT_NOT_FOUND);
}
static result_t dev_list(uint64_t local, objfs_emit_subobject_fn emit, void *arg) {
  if (local != 1) return RESULT_SUCCESS(0);
  emit("Console.out", OBJFS_OBJ_UNKNOWN, make_global_id(g_dev_mount_index, 2), arg);
  emit("Uart.out", OBJFS_OBJ_UNKNOWN, make_global_id(g_dev_mount_index, 3), arg);
  return RESULT_SUCCESS(0);
}
static result_t dev_read(uint64_t local, uint64_t off, void *buf, size_t n, size_t *out) {
  (void)local; (void)off; (void)buf; (void)n;
  if (out) *out = 0;
  return RESULT_SUCCESS(0);
}
static result_t dev_write(uint64_t local, uint64_t off, const void *buf, size_t n, size_t *out) {
  (void)off;
  const char *s = (const char *)buf;
  char *tmp = (char *)kalloc(n + 1);
  if (!tmp) return RESULT_FAILURE(RESULT_NOMEM);
  memcpy(tmp, s, n);
  tmp[n] = '\0';
  if (local == 2) print(tmp, PRINT_FLAG_BOTH);
  else if (local == 3) print(tmp, PRINT_FLAG_UART);
  kfree(tmp);
  if (out) *out = n;
  return RESULT_SUCCESS(0);
}
static result_t dev_stat(uint64_t local, objfs_stat_t *out) {
  (void)local;
  if (!out) return RESULT_FAILURE(RESULT_INVALID);
  out->size = 0;
  out->mode = 0666;
  out->kind = OBJFS_OBJ_UNKNOWN;
  out->nlink = 1;
  return RESULT_SUCCESS(0);
}
static result_t dev_desc(uint64_t local, objfs_object_disk_t *out) {
  if (!out) return RESULT_FAILURE(RESULT_INVALID);
  memset(out, 0, sizeof(*out));
  out->id = make_global_id(g_dev_mount_index, local);
  out->mode = 0666;
  out->kind = (local == 1) ? OBJFS_OBJ_DIR : OBJFS_OBJ_UNKNOWN;
  out->nlink = 1;
  return RESULT_SUCCESS(0);
}
static result_t dev_attr_get(uint64_t local, const char *key, objfs_attr_value_t *out) {
  (void)local; (void)key; (void)out; return RESULT_FAILURE(RESULT_NOT_FOUND);
}
static result_t dev_attr_list(uint64_t local, objfs_emit_attr_fn emit, void *arg) {
  (void)local; (void)emit; (void)arg; return RESULT_SUCCESS(0);
}

static const objfs_vops_t DEV_OPS = {
  .lookup = dev_lookup,
  .list = dev_list,
  .read = dev_read,
  .write = dev_write,
  .stat = dev_stat,
  .desc = dev_desc,
  .attr_get = dev_attr_get,
  .attr_list = dev_attr_list,
};

void objfs_virtual_init_after_mount(void) {
  objfs_fs_t *fs = objfs_global();
  if (!fs) return;
  // Register /Devices under root
  (void)objfs_vreg_register("Devices", fs->sb.root_object_id, &DEV_OPS, NULL);
  // Find Devices mount index (last registered)
  if (g_vmount_count > 0) g_dev_mount_index = (uint16_t)(g_vmount_count - 1);
}


