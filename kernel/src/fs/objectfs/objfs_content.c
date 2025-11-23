#include "objfs.h"
#include <lib/kalloc.h>
#include <lib/memory.h>

result_t objfs_read(uint64_t obj_id, uint64_t off, void *buf, size_t n, size_t *out) {
  if (!buf)
    return RESULT_FAILURE(RESULT_INVALID);
  objfs_fs_t *fs = objfs_global();
  if (!fs)
    return RESULT_FAILURE(RESULT_ERROR);

  // Load object descriptor
  uint32_t bs = fs->sb.block_size;
  uint32_t per = bs / (uint32_t)sizeof(objfs_object_disk_t);
  uint64_t idx = obj_id / per;
  uint64_t within = obj_id % per;
  uint64_t block = fs->sb.object_table_start + idx;
  uint8_t *tbuf = (uint8_t *)kalloc(bs);
  if (!tbuf)
    return RESULT_FAILURE(RESULT_NOMEM);
  result_t rr = objfs_block_read(fs->bc, block, tbuf);
  if (!result_is_ok(rr)) {
    kfree(tbuf);
    return rr;
  }
  objfs_object_disk_t ent = ((objfs_object_disk_t *)tbuf)[within];
  kfree(tbuf);

  if (ent.kind == OBJFS_OBJ_REFERENCE) {
    // follow
    return objfs_read(ent.target_id, off, buf, n, out);
  }
  if (ent.kind != OBJFS_OBJ_FILE)
    return RESULT_FAILURE(RESULT_INVALID);

  uint64_t file_size = ent.size;
  if (off >= file_size) {
    if (out) *out = 0;
    return RESULT_SUCCESS(0);
  }
  uint64_t to_read = n;
  if (off + to_read > file_size)
    to_read = file_size - off;

  uint64_t start_block = ent.data_start_block;
  uint64_t num_blocks = ent.data_num_blocks;
  if (num_blocks == 0) {
    if (out) *out = 0;
    return RESULT_SUCCESS(0);
  }

  // Compute which blocks within the extent to read
  uint64_t first_block_offset = off / bs;
  uint64_t first_block_off_in_block = off % bs;
  if (first_block_offset >= num_blocks)
    return RESULT_FAILURE(RESULT_INVALID);

  uint8_t *blkbuf = (uint8_t *)kalloc(bs);
  if (!blkbuf)
    return RESULT_FAILURE(RESULT_NOMEM);
  size_t copied = 0;
  uint64_t remaining = to_read;
  uint64_t cur_block = start_block + first_block_offset;
  uint64_t block_idx_in_extent = first_block_offset;

  while (remaining > 0 && block_idx_in_extent < num_blocks) {
    result_t r = objfs_block_read(fs->bc, cur_block, blkbuf);
    if (!result_is_ok(r)) {
      kfree(blkbuf);
      return r;
    }
    uint64_t begin = (copied == 0) ? first_block_off_in_block : 0;
    uint64_t can_take = bs - begin;
    if (can_take > remaining) can_take = remaining;
    memcpy((uint8_t *)buf + copied, blkbuf + begin, (size_t)can_take);
    copied += (size_t)can_take;
    remaining -= can_take;
    cur_block++;
    block_idx_in_extent++;
  }
  kfree(blkbuf);
  if (out) *out = copied;
  return RESULT_SUCCESS(0);
}


