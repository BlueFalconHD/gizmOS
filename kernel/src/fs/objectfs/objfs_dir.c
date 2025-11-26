#include "objfs.h"
#include <lib/kalloc.h>
#include <lib/memory.h>
#include <lib/str.h>
#include "objfs_virtual.h"

static inline g_bool is_slash(char c) { return c == '/'; }

static const char *skip_slashes(const char *p) {
  while (*p && is_slash(*p)) p++;
  return p;
}

static const char *next_component(const char *p, char *name_buf, size_t *len) {
  p = skip_slashes(p);
  size_t i = 0;
  while (p[i] && !is_slash(p[i])) {
    if (i < 64) name_buf[i] = p[i];
    i++;
  }
  size_t n = (i < 64) ? i : 64;
  if (n < 64) name_buf[n] = '\0'; else name_buf[63] = '\0';
  if (len) *len = n;
  return p + i;
}

static g_bool name_is_dot(const char *s) { return s[0] == '.' && s[1] == '\0'; }
static g_bool name_is_dotdot(const char *s) { return s[0] == '.' && s[1] == '.' && s[2] == '\0'; }

static result_t for_each_subobject_block(uint64_t head_block,
                                         result_t (*fn)(const objfs_subobject_entry_t *e,
                                                        void *arg),
                                         void *arg) {
  objfs_fs_t *fs = objfs_global();
  if (!fs)
    return RESULT_FAILURE(RESULT_INVALID);
  uint32_t bs = fs->sb.block_size;
  uint8_t *buf = (uint8_t *)kalloc(bs);
  if (!buf)
    return RESULT_FAILURE(RESULT_NOMEM);
  uint64_t blk = head_block;
  while (blk != 0) {
    result_t rr = objfs_block_read(fs->bc, blk, buf);
    if (!result_is_ok(rr)) {
      kfree(buf);
      return rr;
    }
    objfs_subobjects_block_hdr_t *hdr = (objfs_subobjects_block_hdr_t *)buf;
    objfs_subobject_entry_t *entries = (objfs_subobject_entry_t *)(buf + sizeof(*hdr));
    uint32_t count = hdr->count;
    for (uint32_t i = 0; i < count; i++) {
      result_t r = fn(&entries[i], arg);
      if (!result_is_ok(r)) {
        kfree(buf);
        return r;
      }
    }
    blk = hdr->next_block;
  }
  kfree(buf);
  return RESULT_SUCCESS(0);
}

typedef struct {
  const char *name;
  size_t      nlen;
  uint64_t    out_id;
  uint8_t     out_kind;
  g_bool      found;
} find_ctx_t;

static result_t find_cb(const objfs_subobject_entry_t *e, void *arg) {
  find_ctx_t *ctx = (find_ctx_t *)arg;
  size_t en = (size_t)e->name_len;
  if (en > 64) en = 64;
  if (en == ctx->nlen && memcmp(e->name, ctx->name, en) == 0) {
    ctx->out_id = e->subobject_id;
    ctx->out_kind = e->type;
    ctx->found = true;
    return RESULT_FAILURE(RESULT_NOT_FOUND); // sentinel to stop iteration
  }
  return RESULT_SUCCESS(0);
}

static result_t objfs_find_subobject(uint64_t dir_id, const char *name, size_t nlen,
                                     uint64_t *out_id, uint8_t *out_kind) {
  // Ensure directory exists (best-effort)
  (void)objfs_object_stat(dir_id, (objfs_stat_t *)&(objfs_stat_t){0});
  // Load descriptor to get subobjects_idx (avoid extra API coupling)
  objfs_fs_t *fs = objfs_global();
  if (!fs)
    return RESULT_FAILURE(RESULT_INVALID);
  // Load raw descriptor
  uint32_t bs = fs->sb.block_size;
  uint32_t per = bs / (uint32_t)sizeof(objfs_object_disk_t);
  uint64_t idx = dir_id / per;
  uint64_t within = dir_id % per;
  uint64_t block = fs->sb.object_table_start + idx;
  uint8_t *buf = (uint8_t *)kalloc(bs);
  if (!buf)
    return RESULT_FAILURE(RESULT_NOMEM);
  result_t rr = objfs_block_read(fs->bc, block, buf);
  if (!result_is_ok(rr)) {
    kfree(buf);
    return rr;
  }
  objfs_object_disk_t ent = ((objfs_object_disk_t *)buf)[within];
  uint64_t head = ent.subobjects_idx;
  kfree(buf);

  if (head == 0)
    return RESULT_FAILURE(RESULT_NOT_FOUND);
  find_ctx_t ctx = {.name = name, .nlen = nlen, .found = false};
  (void)for_each_subobject_block(head, find_cb, &ctx);
  if (ctx.found) {
    if (out_id) *out_id = ctx.out_id;
    if (out_kind) *out_kind = ctx.out_kind;
    return RESULT_SUCCESS(0);
  }
  // if rf returned RESULT_NOT_FOUND sentinel, it's fine; not found means error
  return RESULT_FAILURE(RESULT_NOT_FOUND);
}

typedef struct {
  objfs_emit_subobject_fn emit;
  void *arg;
} emit_ctx_t;

static result_t objfs_emit_cb(const objfs_subobject_entry_t *e, void *a) {
  emit_ctx_t *ctx = (emit_ctx_t *)a;
  char name[65];
  size_t n = (size_t)e->name_len;
  if (n > 64) n = 64;
  for (size_t i = 0; i < n; i++) name[i] = e->name[i];
  name[n] = '\0';
  // Derive kind from child descriptor capabilities for accurate reporting
  uint8_t derived = e->type;
  objfs_object_disk_t d;
  if (result_is_ok(objfs_read_descriptor(e->subobject_id, &d))) {
    if (d.target_id != 0) derived = OBJFS_OBJ_REFERENCE;
    else if (d.subobjects_idx != 0) derived = OBJFS_OBJ_DIR;
    else if (d.data_num_blocks != 0 || d.size != 0) derived = OBJFS_OBJ_FILE;
    else derived = OBJFS_OBJ_UNKNOWN;
  }
  ctx->emit(name, derived, e->subobject_id, ctx->arg);
  return RESULT_SUCCESS(0);
}

result_t objfs_list_subobjects(uint64_t dir_id, objfs_emit_subobject_fn emit, void *arg) {
  if (!emit)
    return RESULT_FAILURE(RESULT_INVALID);
  // Virtual directory?
  {
    const objfs_vops_t *ops = NULL;
    uint64_t local = 0;
    if (objfs_vreg_resolve(dir_id, &ops, &local)) {
      if (ops && ops->list) return ops->list(local, emit, arg);
      return RESULT_SUCCESS(0);
    }
    // Also inject any mount points that are children of this dir (e.g., under root)
    (void)objfs_vreg_list_mounts(dir_id, emit, arg);
  }
  objfs_fs_t *fs = objfs_global();
  if (!fs)
    return RESULT_FAILURE(RESULT_INVALID);
  // Load descriptor for subobjects_idx
  uint32_t bs = fs->sb.block_size;
  uint32_t per = bs / (uint32_t)sizeof(objfs_object_disk_t);
  uint64_t idx = dir_id / per;
  uint64_t within = dir_id % per;
  uint64_t block = fs->sb.object_table_start + idx;
  uint8_t *buf = (uint8_t *)kalloc(bs);
  if (!buf)
    return RESULT_FAILURE(RESULT_NOMEM);
  result_t rr = objfs_block_read(fs->bc, block, buf);
  if (!result_is_ok(rr)) {
    kfree(buf);
    return rr;
  }
  objfs_object_disk_t ent = ((objfs_object_disk_t *)buf)[within];
  uint64_t head = ent.subobjects_idx;
  kfree(buf);
  if (head == 0)
    return RESULT_SUCCESS(0);

  emit_ctx_t ctx = {.emit = emit, .arg = arg};
  return for_each_subobject_block(head, objfs_emit_cb, &ctx);
}

typedef struct {
  uint64_t cnt;
} count_ctx_t;
static result_t count_cb(const objfs_subobject_entry_t *e, void *a) {
  (void)e;
  count_ctx_t *ctx = (count_ctx_t *)a;
  ctx->cnt++;
  return RESULT_SUCCESS(0);
}

result_t objfs_subobjects_count(uint64_t dir_id, uint64_t *out_count) {
  objfs_fs_t *fs = objfs_global();
  if (!fs)
    return RESULT_FAILURE(RESULT_INVALID);
  uint32_t bs = fs->sb.block_size;
  uint32_t per = bs / (uint32_t)sizeof(objfs_object_disk_t);
  uint64_t idx = dir_id / per;
  uint64_t within = dir_id % per;
  uint64_t block = fs->sb.object_table_start + idx;
  uint8_t *buf = (uint8_t *)kalloc(bs);
  if (!buf)
    return RESULT_FAILURE(RESULT_NOMEM);
  result_t rr = objfs_block_read(fs->bc, block, buf);
  if (!result_is_ok(rr)) {
    kfree(buf);
    return rr;
  }
  objfs_object_disk_t ent = ((objfs_object_disk_t *)buf)[within];
  uint64_t head = ent.subobjects_idx;
  kfree(buf);
  if (head == 0) {
    if (out_count) *out_count = 0;
    return RESULT_SUCCESS(0);
  }
  count_ctx_t c = {.cnt = 0};
  result_t r = for_each_subobject_block(head, count_cb, &c);
  if (result_is_ok(r) && out_count) *out_count = c.cnt;
  return r;
}

typedef struct {
  uint64_t want;
  uint64_t cur;
  char    *name;
  size_t   name_cap;
  uint8_t *out_kind;
  uint64_t *out_id;
  g_bool   found;
} nth_ctx_t;
static result_t nth_cb(const objfs_subobject_entry_t *e, void *a) {
  nth_ctx_t *ctx = (nth_ctx_t *)a;
  if (ctx->found)
    return RESULT_SUCCESS(0);
  if (ctx->cur == ctx->want) {
    if (ctx->name && ctx->name_cap > 0) {
      size_t n = (size_t)e->name_len;
      if (n > 64) n = 64;
      size_t can = (ctx->name_cap > 0) ? (ctx->name_cap - 1) : 0;
      size_t m = (n < can) ? n : can;
      for (size_t i = 0; i < m; i++) ctx->name[i] = e->name[i];
      if (ctx->name_cap > 0) ctx->name[m] = '\0';
    }
    if (ctx->out_kind) *ctx->out_kind = e->type;
    if (ctx->out_id) *ctx->out_id = e->subobject_id;
    ctx->found = true;
    return RESULT_FAILURE(RESULT_NOT_FOUND); // sentinel to stop iteration
  }
  ctx->cur++;
  return RESULT_SUCCESS(0);
}

result_t objfs_subobject_at(uint64_t dir_id, uint64_t index,
                            char *name_buf, size_t name_cap,
                            uint8_t *out_kind, uint64_t *out_id) {
  objfs_fs_t *fs = objfs_global();
  if (!fs)
    return RESULT_FAILURE(RESULT_INVALID);
  uint32_t bs = fs->sb.block_size;
  uint32_t per = bs / (uint32_t)sizeof(objfs_object_disk_t);
  uint64_t idx = dir_id / per;
  uint64_t within = dir_id % per;
  uint64_t block = fs->sb.object_table_start + idx;
  uint8_t *buf = (uint8_t *)kalloc(bs);
  if (!buf)
    return RESULT_FAILURE(RESULT_NOMEM);
  result_t rr = objfs_block_read(fs->bc, block, buf);
  if (!result_is_ok(rr)) {
    kfree(buf);
    return rr;
  }
  objfs_object_disk_t ent = ((objfs_object_disk_t *)buf)[within];
  uint64_t head = ent.subobjects_idx;
  kfree(buf);
  if (head == 0)
    return RESULT_FAILURE(RESULT_NOT_FOUND);
  nth_ctx_t nctx = {
      .want = index, .cur = 0, .name = name_buf, .name_cap = name_cap,
      .out_kind = out_kind, .out_id = out_id, .found = false};
  (void)for_each_subobject_block(head, nth_cb, &nctx);
  if (!nctx.found) return RESULT_FAILURE(RESULT_NOT_FOUND);
  // Derive kind from descriptor if requested
  if (out_kind) {
    objfs_object_disk_t d;
    if (result_is_ok(objfs_read_descriptor(*out_id, &d))) {
      if (d.target_id != 0) *out_kind = OBJFS_OBJ_REFERENCE;
      else if (d.subobjects_idx != 0) *out_kind = OBJFS_OBJ_DIR;
      else if (d.data_num_blocks != 0 || d.size != 0) *out_kind = OBJFS_OBJ_FILE;
      else *out_kind = OBJFS_OBJ_UNKNOWN;
    }
  }
  return RESULT_SUCCESS(0);
}
result_t objfs_lookup_path(const char *path, uint64_t *out_obj_id) {
  if (!path || !out_obj_id)
    return RESULT_FAILURE(RESULT_INVALID);
  objfs_fs_t *fs = objfs_global();
  if (!fs)
    return RESULT_FAILURE(RESULT_ERROR);
  uint64_t cur = fs->sb.root_object_id;
  const char *p = path;
  if (is_slash(*p))
    p = skip_slashes(p);
  if (!*p) {
    *out_obj_id = cur;
    return RESULT_SUCCESS(0);
  }
  char name[65];
  size_t nlen = 0;
  while (*p) {
    p = next_component(p, name, &nlen);
    if (nlen == 0) break;
    if (name_is_dot(name)) {
      // no-op
    } else if (name_is_dotdot(name)) {
      // no parent tracking; reset to root
      cur = fs->sb.root_object_id;
    } else {
      uint64_t sub = 0;
      uint8_t  kind = 0;
      result_t r = objfs_find_subobject(cur, name, nlen, &sub, &kind);
      if (!result_is_ok(r))
        return r;
      // resolve reference hop if needed via descriptor field (capability-based)
      uint64_t resolved = sub;
      objfs_object_disk_t d;
      // load raw descriptor
      uint32_t bs = fs->sb.block_size;
      uint32_t per = bs / (uint32_t)sizeof(objfs_object_disk_t);
      uint64_t idx = sub / per;
      uint64_t within = sub % per;
      uint64_t block = fs->sb.object_table_start + idx;
      uint8_t *buf = (uint8_t *)kalloc(bs);
      if (!buf)
        return RESULT_FAILURE(RESULT_NOMEM);
      result_t rb = objfs_block_read(fs->bc, block, buf);
      if (!result_is_ok(rb)) {
        kfree(buf);
        return rb;
      }
      d = ((objfs_object_disk_t *)buf)[within];
      kfree(buf);
      if (d.target_id != 0) resolved = d.target_id;
      cur = resolved;
    }
    p = skip_slashes(p);
  }
  *out_obj_id = cur;
  return RESULT_SUCCESS(0);
}


