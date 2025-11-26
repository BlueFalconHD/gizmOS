#include "objfs.h"
#include "objfs_virtual.h"
#include <lib/kalloc.h>
#include <lib/memory.h>
#include <lib/str.h>

// Allocator functions declared in objfs.h

static result_t objfs_load_desc(uint64_t obj_id, objfs_object_disk_t *out, uint64_t *block_out, uint32_t *index_in_block_out) {
  objfs_fs_t *fs = objfs_global();
  if (!fs || !out)
    return RESULT_FAILURE(RESULT_INVALID);
  uint32_t bs = fs->sb.block_size;
  uint32_t per = bs / (uint32_t)sizeof(objfs_object_disk_t);
  if (per == 0)
    return RESULT_FAILURE(RESULT_INVALID);
  uint64_t idx = obj_id / per;
  uint32_t within = (uint32_t)(obj_id % per);
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
  memcpy(out, &ent, sizeof(*out));
  kfree(buf);
  if (block_out) *block_out = block;
  if (index_in_block_out) *index_in_block_out = within;
  return RESULT_SUCCESS(0);
}

static result_t objfs_store_desc(uint64_t obj_id, const objfs_object_disk_t *in) {
  objfs_fs_t *fs = objfs_global();
  if (!fs || !in)
    return RESULT_FAILURE(RESULT_INVALID);
  uint32_t bs = fs->sb.block_size;
  uint32_t per = bs / (uint32_t)sizeof(objfs_object_disk_t);
  uint64_t idx = obj_id / per;
  uint32_t within = (uint32_t)(obj_id % per);
  uint64_t block = fs->sb.object_table_start + idx;
  uint8_t *buf = (uint8_t *)kalloc(bs);
  if (!buf)
    return RESULT_FAILURE(RESULT_NOMEM);
  result_t rr = objfs_block_read(fs->bc, block, buf);
  if (!result_is_ok(rr)) {
    kfree(buf);
    return rr;
  }
  ((objfs_object_disk_t *)buf)[within] = *in;
  result_t rw = objfs_block_write(fs->bc, block, buf);
  kfree(buf);
  return rw;
}

result_t objfs_write_bytes(uint64_t obj_id, uint64_t off, const void *buf, size_t n, size_t *out) {
  if (!buf)
    return RESULT_FAILURE(RESULT_INVALID);
  // Virtual object?
  {
    const objfs_vops_t *ops = NULL;
    uint64_t local = 0;
    if (objfs_vreg_resolve(obj_id, &ops, &local)) {
      if (ops && ops->write) return ops->write(local, off, buf, n, out);
      if (out) *out = 0;
      return RESULT_SUCCESS(0);
    }
  }
  objfs_fs_t *fs = objfs_global();
  if (!fs)
    return RESULT_FAILURE(RESULT_ERROR);
  objfs_object_disk_t ent;
  result_t rl = objfs_load_desc(obj_id, &ent, NULL, NULL);
  if (!result_is_ok(rl))
    return rl;
  if (ent.target_id != 0) {
    return objfs_write_bytes(ent.target_id, off, buf, n, out);
  }
  uint32_t bs = fs->sb.block_size;
  uint64_t cur_cap = (uint64_t)ent.data_num_blocks * (uint64_t)bs;
  uint64_t end_off = off + n;
  if (end_off > cur_cap) {
    // Need to grow extent; try allocate new larger run and migrate if cannot extend in place
    uint64_t need_bytes = end_off;
    uint32_t need_blocks = (uint32_t)((need_bytes + bs - 1) / bs);
    // Try to allocate a fresh run and migrate existing data
    uint64_t new_first = 0;
    result_t ra = objfs_alloc_run(need_blocks, &new_first);
    if (!result_is_ok(ra))
      return ra;
    // Copy existing data if any
    if (ent.data_num_blocks > 0) {
      uint8_t *tmp = (uint8_t *)kalloc(bs);
      if (!tmp) return RESULT_FAILURE(RESULT_NOMEM);
      uint64_t src = ent.data_start_block;
      uint64_t dst = new_first;
      for (uint32_t i = 0; i < ent.data_num_blocks; i++) {
        result_t rr = objfs_block_read(fs->bc, src + i, tmp);
        if (!result_is_ok(rr)) { kfree(tmp); return rr; }
        result_t rw = objfs_block_write(fs->bc, dst + i, tmp);
        if (!result_is_ok(rw)) { kfree(tmp); return rw; }
      }
      kfree(tmp);
      // free old run
      objfs_free_run(ent.data_start_block, ent.data_num_blocks);
    }
    ent.data_start_block = new_first;
    ent.data_num_blocks = need_blocks;
    if (ent.size < end_off) ent.size = end_off;
    result_t rs = objfs_store_desc(obj_id, &ent);
    if (!result_is_ok(rs)) return rs;
  } else {
    if (ent.size < end_off) {
      ent.size = end_off;
      result_t rs = objfs_store_desc(obj_id, &ent);
      if (!result_is_ok(rs)) return rs;
    }
  }
  // Write bytes into extent
  size_t total = 0;
  const uint8_t *src = (const uint8_t *)buf;
  uint8_t *blk = (uint8_t *)kalloc(bs);
  if (!blk) return RESULT_FAILURE(RESULT_NOMEM);
  uint64_t start_block = ent.data_start_block + (off / bs);
  uint64_t block_off = off % bs;
  uint64_t remaining = n;
  uint64_t cur_block = start_block;
  while (remaining > 0) {
    result_t rr = objfs_block_read(fs->bc, cur_block, blk);
    if (!result_is_ok(rr)) { kfree(blk); return rr; }
    uint64_t can = bs - block_off;
    if (can > remaining) can = remaining;
    memcpy(blk + block_off, src + total, (size_t)can);
    result_t rw = objfs_block_write(fs->bc, cur_block, blk);
    if (!result_is_ok(rw)) { kfree(blk); return rw; }
    total += (size_t)can;
    remaining -= can;
    cur_block++;
    block_off = 0;
  }
  kfree(blk);
  if (out) *out = total;
  return RESULT_SUCCESS(0);
}

static result_t ensure_subobjects_head(objfs_object_disk_t *dir, uint64_t dir_id) {
  objfs_fs_t *fs = objfs_global();
  if (!fs) return RESULT_FAILURE(RESULT_ERROR);
  if (dir->subobjects_idx != 0) return RESULT_SUCCESS(0);
  // allocate one block
  uint64_t blk = 0;
  result_t ra = objfs_alloc_run(1, &blk);
  if (!result_is_ok(ra)) return ra;
  // initialize empty header
  uint8_t *buf = (uint8_t *)kalloc(fs->sb.block_size);
  if (!buf) return RESULT_FAILURE(RESULT_NOMEM);
  memset(buf, 0, fs->sb.block_size);
  objfs_subobjects_block_hdr_t *hdr = (objfs_subobjects_block_hdr_t *)buf;
  hdr->count = 0;
  hdr->next_block = 0;
  result_t rw = objfs_block_write(fs->bc, blk, buf);
  kfree(buf);
  if (!result_is_ok(rw)) return rw;
  dir->subobjects_idx = blk;
  return objfs_store_desc(dir_id, dir);
}

static result_t add_subobject_entry(uint64_t head, const char *name, uint8_t kind, uint64_t subobject_id) {
  objfs_fs_t *fs = objfs_global();
  uint32_t bs = fs->sb.block_size;
  uint8_t *buf = (uint8_t *)kalloc(bs);
  if (!buf) return RESULT_FAILURE(RESULT_NOMEM);
  uint64_t blk = head;
  while (true) {
    result_t rr = objfs_block_read(fs->bc, blk, buf);
    if (!result_is_ok(rr)) { kfree(buf); return rr; }
    objfs_subobjects_block_hdr_t *hdr = (objfs_subobjects_block_hdr_t *)buf;
    objfs_subobject_entry_t *ents = (objfs_subobject_entry_t *)(buf + sizeof(*hdr));
    uint32_t maxents = (bs - sizeof(*hdr)) / sizeof(objfs_subobject_entry_t);
    if (hdr->count < maxents) {
      objfs_subobject_entry_t *e = &ents[hdr->count];
      size_t n = strlen(name);
      if (n > 64) n = 64;
      e->name_len = (uint8_t)n;
      e->type = kind;
      e->_pad16 = 0;
      for (size_t i = 0; i < 64; i++) e->name[i] = (i < n) ? name[i] : '\0';
      e->subobject_id = subobject_id;
      hdr->count++;
      result_t rw = objfs_block_write(fs->bc, blk, buf);
      kfree(buf);
      return rw;
    }
    if (hdr->next_block == 0) {
      // allocate a new block and chain
      uint64_t nb = 0;
      result_t ra = objfs_alloc_run(1, &nb);
      if (!result_is_ok(ra)) { kfree(buf); return ra; }
      hdr->next_block = nb;
      result_t rw0 = objfs_block_write(fs->bc, blk, buf);
      if (!result_is_ok(rw0)) { kfree(buf); return rw0; }
      // init new block
      memset(buf, 0, bs);
      objfs_subobjects_block_hdr_t *nh = (objfs_subobjects_block_hdr_t *)buf;
      nh->count = 0; nh->next_block = 0;
      result_t rw1 = objfs_block_write(fs->bc, nb, buf);
      if (!result_is_ok(rw1)) { kfree(buf); return rw1; }
      blk = nb;
      continue;
    } else {
      blk = hdr->next_block;
    }
  }
}

static result_t find_free_object_slot(uint64_t *out_id) {
  objfs_fs_t *fs = objfs_global();
  if (!fs) return RESULT_FAILURE(RESULT_ERROR);
  uint32_t bs = fs->sb.block_size;
  uint32_t per = bs / (uint32_t)sizeof(objfs_object_disk_t);
  uint8_t *buf = (uint8_t *)kalloc(bs);
  if (!buf) return RESULT_FAILURE(RESULT_NOMEM);
  for (uint64_t b = 0; b < fs->sb.object_table_blocks; b++) {
    uint64_t blk = fs->sb.object_table_start + b;
    result_t rr = objfs_block_read(fs->bc, blk, buf);
    if (!result_is_ok(rr)) { kfree(buf); return rr; }
    objfs_object_disk_t *ents = (objfs_object_disk_t *)buf;
    for (uint32_t i = 0; i < per; i++) {
      uint64_t id = b * per + i;
      if (id == 0) continue; // keep root intact
      // Consider a slot free if link count is zero (builder uses nlink=0 for empty slots)
      if (ents[i].nlink == 0) {
        kfree(buf);
        *out_id = id;
        return RESULT_SUCCESS(0);
      }
    }
  }
  kfree(buf);
  return RESULT_FAILURE(RESULT_NOT_FOUND);
}

result_t objfs_create(uint64_t parent_dir_id, const char *name, uint16_t mode, uint8_t kind, uint64_t *out_obj_id) {
  if (!name)
    return RESULT_FAILURE(RESULT_INVALID);
  objfs_object_disk_t dir;
  result_t rl = objfs_load_desc(parent_dir_id, &dir, NULL, NULL);
  if (!result_is_ok(rl)) return rl;
  // Allocate object slot
  uint64_t new_id = 0;
  result_t rs = find_free_object_slot(&new_id);
  if (!result_is_ok(rs)) return rs;
  // Initialize descriptor
  objfs_object_disk_t nd;
  memset(&nd, 0, sizeof(nd));
  nd.id = new_id;
  // Ignore requested kind for capabilities-based model; start unknown and derive on use
  nd.kind = OBJFS_OBJ_UNKNOWN;
  nd.mode = mode;
  nd.nlink = 1;
  nd.size = 0;
  result_t rw = objfs_store_desc(new_id, &nd);
  if (!result_is_ok(rw)) return rw;
  // Ensure dir has subobjects head, then add entry
  result_t re = ensure_subobjects_head(&dir, parent_dir_id);
  if (!result_is_ok(re)) return re;
  // Emit unknown type in index; consumers will derive kind from descriptor fields
  result_t ra = add_subobject_entry(dir.subobjects_idx, name, OBJFS_OBJ_UNKNOWN, new_id);
  if (!result_is_ok(ra)) return ra;
  if (out_obj_id) *out_obj_id = new_id;
  return RESULT_SUCCESS(0);
}

result_t objfs_link(uint64_t parent_dir_id, const char *name, uint64_t target_id) {
  objfs_object_disk_t dir;
  result_t rl = objfs_load_desc(parent_dir_id, &dir, NULL, NULL);
  if (!result_is_ok(rl)) return rl;
  result_t re = ensure_subobjects_head(&dir, parent_dir_id);
  if (!result_is_ok(re)) return re;
  objfs_object_disk_t t;
  if (!result_is_ok(objfs_load_desc(target_id, &t, NULL, NULL)))
    return RESULT_FAILURE(RESULT_NOT_FOUND);
  // Derive a display kind for the index from capabilities
  uint8_t derived = OBJFS_OBJ_UNKNOWN;
  if (t.target_id != 0) derived = OBJFS_OBJ_REFERENCE;
  else if (t.subobjects_idx != 0) derived = OBJFS_OBJ_DIR;
  else if (t.data_num_blocks != 0 || t.size != 0) derived = OBJFS_OBJ_FILE;
  return add_subobject_entry(dir.subobjects_idx, name, derived, target_id);
}

static result_t update_subobject_by_name(uint64_t head, const char *name,
                                         g_bool remove, const char *rename_to) {
  objfs_fs_t *fs = objfs_global();
  uint32_t bs = fs->sb.block_size;
  uint8_t *buf = (uint8_t *)kalloc(bs);
  if (!buf) return RESULT_FAILURE(RESULT_NOMEM);
  uint64_t blk = head;
  size_t nlen = strlen(name);
  while (blk) {
    result_t rr = objfs_block_read(fs->bc, blk, buf);
    if (!result_is_ok(rr)) { kfree(buf); return rr; }
    objfs_subobjects_block_hdr_t *hdr = (objfs_subobjects_block_hdr_t *)buf;
    objfs_subobject_entry_t *ents = (objfs_subobject_entry_t *)(buf + sizeof(*hdr));
    for (uint32_t i = 0; i < hdr->count; i++) {
      size_t en = ents[i].name_len;
      if (en == nlen && memcmp(ents[i].name, name, nlen) == 0) {
        if (remove) {
          // shift tail entries up
          for (uint32_t j = i + 1; j < hdr->count; j++) {
            ents[j - 1] = ents[j];
          }
          hdr->count--;
        } else if (rename_to) {
          size_t newn = strlen(rename_to);
          if (newn > 64) newn = 64;
          ents[i].name_len = (uint8_t)newn;
          for (size_t k = 0; k < 64; k++)
            ents[i].name[k] = (k < newn) ? rename_to[k] : '\0';
        }
        result_t rw = objfs_block_write(fs->bc, blk, buf);
        kfree(buf);
        return rw;
      }
    }
    blk = hdr->next_block;
  }
  kfree(buf);
  return RESULT_FAILURE(RESULT_NOT_FOUND);
}

result_t objfs_unlink(uint64_t parent_dir_id, const char *name) {
  objfs_object_disk_t dir;
  result_t rl = objfs_load_desc(parent_dir_id, &dir, NULL, NULL);
  if (!result_is_ok(rl)) return rl;
  if (dir.subobjects_idx == 0) return RESULT_FAILURE(RESULT_NOT_FOUND);
  return update_subobject_by_name(dir.subobjects_idx, name, true, NULL);
}

result_t objfs_rename(uint64_t parent_dir_id, const char *old_name, const char *new_name) {
  objfs_object_disk_t dir;
  result_t rl = objfs_load_desc(parent_dir_id, &dir, NULL, NULL);
  if (!result_is_ok(rl)) return rl;
  if (dir.subobjects_idx == 0) return RESULT_FAILURE(RESULT_NOT_FOUND);
  return update_subobject_by_name(dir.subobjects_idx, old_name, false, new_name);
}

static result_t set_attr_core(uint64_t obj_id, const char *key, uint8_t type, const void *payload, uint16_t vlen) {
  objfs_fs_t *fs = objfs_global();
  if (!fs) return RESULT_FAILURE(RESULT_ERROR);
  // load descriptor to get attrs_head
  objfs_object_disk_t ent;
  uint64_t blk = 0; uint32_t idx = 0;
  result_t rl = objfs_load_desc(obj_id, &ent, &blk, &idx);
  if (!result_is_ok(rl)) return rl;
  if (ent.attrs_head == 0) {
    uint64_t nb = 0;
    result_t ra = objfs_alloc_run(1, &nb);
    if (!result_is_ok(ra)) return ra;
    // init attrs block
    uint8_t *buf = (uint8_t *)kalloc(fs->sb.block_size);
    if (!buf) return RESULT_FAILURE(RESULT_NOMEM);
    memset(buf, 0, fs->sb.block_size);
    objfs_attrs_block_hdr_t *hdr = (objfs_attrs_block_hdr_t *)buf;
    hdr->count = 0; hdr->next_block = 0;
    result_t rw = objfs_block_write(fs->bc, nb, buf);
    kfree(buf);
    if (!result_is_ok(rw)) return rw;
    ent.attrs_head = nb;
    result_t rs = objfs_store_desc(obj_id, &ent);
    if (!result_is_ok(rs)) return rs;
  }
  // append or update in first block with space
  uint32_t bs = fs->sb.block_size;
  uint8_t *buf = (uint8_t *)kalloc(bs);
  if (!buf) return RESULT_FAILURE(RESULT_NOMEM);
  uint64_t cur = ent.attrs_head;
  while (true) {
    result_t rr = objfs_block_read(fs->bc, cur, buf);
    if (!result_is_ok(rr)) { kfree(buf); return rr; }
    objfs_attrs_block_hdr_t *hdr = (objfs_attrs_block_hdr_t *)buf;
    // try update if key exists
    uint8_t *p = buf + sizeof(*hdr);
    for (uint32_t i = 0; i < hdr->count; i++) {
      objfs_attr_entry_hdr_t *eh = (objfs_attr_entry_hdr_t *)p;
      size_t klen = eh->key_len;
      if (klen > 64) klen = 64;
      if (klen == strlen(key) && memcmp(eh->key, key, klen) == 0) {
        // replace in place if same size/type or mark not supported; for MVP, just overwrite type/val if size fits
        uint8_t *payload_ptr = (uint8_t *)(eh + 1);
        if (type == OBJFS_ATTR_STR) {
          size_t can = (size_t)eh->vlen;
          size_t n = vlen;
          if (n > can) n = can;
          memcpy(payload_ptr, payload, n);
          if (n < can) memset(payload_ptr + n, 0, can - n);
          eh->type = type;
        } else if (type == OBJFS_ATTR_INT) {
          int64_t v = 0;
          memcpy(&v, payload, sizeof(int64_t));
          memcpy(payload_ptr, &v, sizeof(int64_t));
          eh->type = type;
        } else {
          memcpy(payload_ptr, payload, sizeof(uint8_t));
          eh->type = type;
        }
        result_t rw = objfs_block_write(fs->bc, cur, buf);
        kfree(buf);
        return rw;
      }
      // advance to next entry
      size_t advance = sizeof(*eh);
      if (eh->type == OBJFS_ATTR_STR) advance += eh->vlen;
      else if (eh->type == OBJFS_ATTR_INT) advance += sizeof(int64_t);
      else advance += sizeof(uint8_t);
      if (advance % 8) advance += (8 - (advance % 8));
      p += advance;
    }
    // append a new entry if space
    size_t used = (size_t)(p - buf);
    size_t need = sizeof(objfs_attr_entry_hdr_t);
    if (type == OBJFS_ATTR_STR) need += vlen; else if (type == OBJFS_ATTR_INT) need += sizeof(int64_t); else need += sizeof(uint8_t);
    if (need % 8) need += (8 - (need % 8));
    if (used + need <= bs) {
      objfs_attr_entry_hdr_t *eh = (objfs_attr_entry_hdr_t *)p;
      size_t klen = strlen(key); if (klen > 64) klen = 64;
      eh->key_len = (uint8_t)klen;
      eh->type = type;
      eh->vlen = (type == OBJFS_ATTR_STR) ? vlen : 0;
      for (size_t i = 0; i < 64; i++) eh->key[i] = (i < klen) ? key[i] : '\0';
      uint8_t *payload_ptr = (uint8_t *)(eh + 1);
      if (type == OBJFS_ATTR_STR) memcpy(payload_ptr, payload, vlen);
      else if (type == OBJFS_ATTR_INT) memcpy(payload_ptr, payload, sizeof(int64_t));
      else memcpy(payload_ptr, payload, sizeof(uint8_t));
      hdr->count++;
      result_t rw = objfs_block_write(fs->bc, cur, buf);
      kfree(buf);
      return rw;
    }
    // allocate and chain next block
    if (hdr->next_block == 0) {
      uint64_t nb = 0;
      result_t ra = objfs_alloc_run(1, &nb);
      if (!result_is_ok(ra)) { kfree(buf); return ra; }
      hdr->next_block = nb;
      result_t rw0 = objfs_block_write(fs->bc, cur, buf);
      if (!result_is_ok(rw0)) { kfree(buf); return rw0; }
      memset(buf, 0, bs);
      objfs_attrs_block_hdr_t *nh = (objfs_attrs_block_hdr_t *)buf;
      nh->count = 0; nh->next_block = 0;
      result_t rw1 = objfs_block_write(fs->bc, nb, buf);
      if (!result_is_ok(rw1)) { kfree(buf); return rw1; }
      cur = nb;
      continue;
    } else {
      cur = hdr->next_block;
    }
  }
}

result_t objfs_set_attr_str(uint64_t obj_id, const char *key, const char *value) {
  return set_attr_core(obj_id, key, OBJFS_ATTR_STR, value, (uint16_t)strlen(value));
}
result_t objfs_set_attr_int(uint64_t obj_id, const char *key, int64_t value) {
  return set_attr_core(obj_id, key, OBJFS_ATTR_INT, &value, 0);
}
result_t objfs_set_attr_bool(uint64_t obj_id, const char *key, uint8_t value) {
  return set_attr_core(obj_id, key, OBJFS_ATTR_BOOL, &value, 0);
}


