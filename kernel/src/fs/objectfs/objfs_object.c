#include "objfs.h"
#include <lib/kalloc.h>
#include <lib/memory.h>
#include <lib/str.h>

static result_t objfs_load_object(uint64_t obj_id, objfs_object_disk_t *out) {
  objfs_fs_t *fs = objfs_global();
  if (!fs || !out)
    return RESULT_FAILURE(RESULT_INVALID);
  uint32_t bs = fs->sb.block_size;
  uint32_t per = bs / (uint32_t)sizeof(objfs_object_disk_t);
  if (per == 0)
    return RESULT_FAILURE(RESULT_INVALID);
  uint64_t idx = obj_id / per;
  uint64_t within = obj_id % per;
  uint64_t block = fs->sb.object_table_start + idx;

  uint8_t *buf = (uint8_t *)kalloc(bs);
  if (!buf)
    return RESULT_FAILURE(RESULT_NOMEM);
  result_t rr = objfs_block_read(fs->bc, block, buf);
  if (!result_is_ok(rr)) {
    kfree(buf);
    return rr;
  }
  objfs_object_disk_t *table = (objfs_object_disk_t *)buf;
  objfs_object_disk_t ent = table[within];
  memcpy(out, &ent, sizeof(*out));
  kfree(buf);
  return RESULT_SUCCESS(0);
}

static result_t objfs_resolve_reference(uint64_t obj_id, uint64_t *out_id) {
  uint64_t cur = obj_id;
  for (int depth = 0; depth < 16; depth++) {
    objfs_object_disk_t d;
    result_t r = objfs_load_object(cur, &d);
    if (!result_is_ok(r))
      return r;
    if (d.kind != OBJFS_OBJ_REFERENCE) {
      *out_id = cur;
      return RESULT_SUCCESS(0);
    }
    // follow
    if (d.target_id == cur)
      return RESULT_FAILURE(RESULT_ERROR);
    cur = d.target_id;
  }
  return RESULT_FAILURE(RESULT_ERROR);
}

result_t objfs_object_stat(uint64_t obj_id, objfs_stat_t *out) {
  if (!out)
    return RESULT_FAILURE(RESULT_INVALID);
  uint64_t id = 0;
  result_t rr = objfs_resolve_reference(obj_id, &id);
  if (!result_is_ok(rr))
    return rr;
  objfs_object_disk_t d;
  result_t r = objfs_load_object(id, &d);
  if (!result_is_ok(r))
    return r;
  out->size = d.size;
  out->mode = d.mode;
  out->kind = d.kind;
  out->nlink = d.nlink;
  return RESULT_SUCCESS(0);
}


