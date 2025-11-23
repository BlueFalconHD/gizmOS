#include "objfs.h"
#include <lib/kalloc.h>
#include <lib/memory.h>
#include <lib/str.h>

typedef struct {
  const char *key;
  size_t      klen;
  objfs_attr_value_t *out;
  char       *strbuf;
  size_t      strcap;
  size_t     *out_strlen;
  g_bool      found;
} find_attr_ctx_t;

static result_t attrs_for_each(uint64_t head_block,
                               result_t (*fn)(const objfs_attr_entry_hdr_t *h, const uint8_t *payload, void *arg),
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
    objfs_attrs_block_hdr_t *hdr = (objfs_attrs_block_hdr_t *)buf;
    uint8_t *p = buf + sizeof(*hdr);
    for (uint32_t i = 0; i < hdr->count; i++) {
      objfs_attr_entry_hdr_t *eh = (objfs_attr_entry_hdr_t *)p;
      uint8_t type = eh->type;
      uint16_t vlen = eh->vlen;
      const uint8_t *payload = (const uint8_t *)(eh + 1);
      result_t r = fn(eh, payload, arg);
      if (!result_is_ok(r)) {
        kfree(buf);
        return r;
      }
      // advance p
      size_t advance = sizeof(*eh);
      if (type == OBJFS_ATTR_STR) {
        advance += vlen;
      } else if (type == OBJFS_ATTR_INT) {
        advance += sizeof(int64_t);
      } else {
        advance += sizeof(uint8_t);
      }
      // align to 8 for simplicity
      if (advance % 8) advance += (8 - (advance % 8));
      p += advance;
      if (p >= buf + bs)
        break;
    }
    blk = hdr->next_block;
  }
  kfree(buf);
  return RESULT_SUCCESS(0);
}

static result_t find_attr_cb(const objfs_attr_entry_hdr_t *h, const uint8_t *payload, void *arg) {
  find_attr_ctx_t *ctx = (find_attr_ctx_t *)arg;
  size_t hk = (size_t)h->key_len;
  if (hk > 64) hk = 64;
  if (hk == ctx->klen && memcmp(h->key, ctx->key, hk) == 0) {
    if (ctx->out) {
      ctx->out->type = h->type;
      if (h->type == OBJFS_ATTR_INT) {
        int64_t v = 0;
        memcpy(&v, payload, sizeof(int64_t));
        ctx->out->v.i64 = v;
      } else if (h->type == OBJFS_ATTR_BOOL) {
        uint8_t b = 0;
        memcpy(&b, payload, sizeof(uint8_t));
        ctx->out->v.b = b;
      }
    }
    if (h->type == OBJFS_ATTR_STR && ctx->strbuf && ctx->strcap > 0) {
      size_t n = h->vlen;
      if (n > ctx->strcap - 1) n = ctx->strcap - 1;
      memcpy(ctx->strbuf, payload, n);
      ctx->strbuf[n] = '\0';
      if (ctx->out_strlen) *ctx->out_strlen = n;
    }
    ctx->found = true;
    return RESULT_FAILURE(RESULT_NOT_FOUND); // sentinel stop
  }
  return RESULT_SUCCESS(0);
}

// Helper to load attrs_head from object descriptor
static result_t get_attrs_head(uint64_t obj_id, uint64_t *out_head) {
  objfs_fs_t *fs = objfs_global();
  if (!fs)
    return RESULT_FAILURE(RESULT_INVALID);
  uint32_t bs = fs->sb.block_size;
  uint32_t per = bs / (uint32_t)sizeof(objfs_object_disk_t);
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
  objfs_object_disk_t ent = ((objfs_object_disk_t *)buf)[within];
  kfree(buf);
  *out_head = ent.attrs_head;
  return RESULT_SUCCESS(0);
}

result_t objfs_attr_get(uint64_t obj_id, const char *key, objfs_attr_value_t *out) {
  if (!key)
    return RESULT_FAILURE(RESULT_INVALID);
  uint64_t head = 0;
  result_t rg = get_attrs_head(obj_id, &head);
  if (!result_is_ok(rg))
    return rg;
  if (head == 0)
    return RESULT_FAILURE(RESULT_NOT_FOUND);
  find_attr_ctx_t ctx = {
      .key = key, .klen = strlen(key), .out = out,
      .strbuf = NULL, .strcap = 0, .out_strlen = NULL, .found = false};
  result_t r = attrs_for_each(head, find_attr_cb, &ctx);
  if (ctx.found)
    return RESULT_SUCCESS(0);
  return RESULT_FAILURE(RESULT_NOT_FOUND);
}

result_t objfs_attr_get_str(uint64_t obj_id, const char *key, char *buf, size_t cap, size_t *out_len) {
  if (!key || !buf || cap == 0)
    return RESULT_FAILURE(RESULT_INVALID);
  uint64_t head = 0;
  result_t rg = get_attrs_head(obj_id, &head);
  if (!result_is_ok(rg))
    return rg;
  if (head == 0)
    return RESULT_FAILURE(RESULT_NOT_FOUND);
  find_attr_ctx_t ctx = {
      .key = key, .klen = strlen(key), .out = NULL,
      .strbuf = buf, .strcap = cap, .out_strlen = out_len, .found = false};
  result_t r = attrs_for_each(head, find_attr_cb, &ctx);
  if (ctx.found)
    return RESULT_SUCCESS(0);
  return RESULT_FAILURE(RESULT_NOT_FOUND);
}

typedef struct {
  objfs_emit_attr_fn emit;
  void *arg;
} list_ctx_t;

static result_t list_attr_cb(const objfs_attr_entry_hdr_t *h, const uint8_t *payload, void *arg) {
  (void)payload;
  list_ctx_t *ctx = (list_ctx_t *)arg;
  char key[65];
  size_t n = (size_t)h->key_len;
  if (n > 64) n = 64;
  for (size_t i = 0; i < n; i++) key[i] = h->key[i];
  key[n] = '\0';
  ctx->emit(key, h->type, ctx->arg);
  return RESULT_SUCCESS(0);
}

result_t objfs_list_attrs(uint64_t obj_id, objfs_emit_attr_fn emit, void *arg) {
  if (!emit)
    return RESULT_FAILURE(RESULT_INVALID);
  uint64_t head = 0;
  result_t rg = get_attrs_head(obj_id, &head);
  if (!result_is_ok(rg))
    return rg;
  if (head == 0)
    return RESULT_SUCCESS(0);
  list_ctx_t ctx = {.emit = emit, .arg = arg};
  return attrs_for_each(head, list_attr_cb, &ctx);
}


