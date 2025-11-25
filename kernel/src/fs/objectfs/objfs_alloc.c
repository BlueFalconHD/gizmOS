#include "objfs.h"
#include <lib/kalloc.h>
#include <lib/memory.h>

static inline void set_bit(uint8_t *buf, uint64_t bit, g_bool free) {
  uint64_t byte = bit / 8;
  uint8_t mask = (uint8_t)(1u << (bit % 8));
  if (free)
    buf[byte] |= mask;   // 1=free
  else
    buf[byte] &= (uint8_t)~mask; // 0=used
}

static inline g_bool get_bit(const uint8_t *buf, uint64_t bit) {
  uint64_t byte = bit / 8;
  uint8_t mask = (uint8_t)(1u << (bit % 8));
  return (buf[byte] & mask) != 0;
}

// Scan free map for a contiguous run of free blocks of length 'need'
result_t objfs_alloc_run(uint32_t need, uint64_t *out_first) {
  objfs_fs_t *fs = objfs_global();
  if (!fs || need == 0)
    return RESULT_FAILURE(RESULT_INVALID);
  uint32_t bs = fs->sb.block_size;
  uint64_t total = fs->blocks_total;
  uint64_t freemap_bits = fs->sb.free_map_blocks * (uint64_t)(bs * 8);
  if (freemap_bits < total)
    total = freemap_bits;

  // iterate through free map blocks
  uint8_t *buf = (uint8_t *)kalloc(bs);
  if (!buf)
    return RESULT_FAILURE(RESULT_NOMEM);
  uint64_t run_start = 0;
  uint32_t run_len = 0;
  uint64_t inspected = 0;
  for (uint64_t mblk = 0; mblk < fs->sb.free_map_blocks; mblk++) {
    uint64_t map_block_idx = fs->sb.free_map_start + mblk;
    result_t rr = objfs_block_read(fs->bc, map_block_idx, buf);
    if (!result_is_ok(rr)) {
      kfree(buf);
      return rr;
    }
    for (uint64_t bit = 0; bit < (uint64_t)bs * 8 && inspected < total; bit++, inspected++) {
      uint64_t block_idx = inspected; // bit 0 == block 0
      g_bool is_free = get_bit(buf, bit);
      if (is_free) {
        if (run_len == 0) run_start = block_idx;
        run_len++;
        if (run_len >= need) {
          // mark allocated (set to used=0)
          // we may span across map blocks; re-scan and write back
          uint64_t to_mark = need;
          uint64_t mark_at = run_start;
          while (to_mark > 0) {
            uint64_t mb = (mark_at / (uint64_t)(bs * 8));
            uint64_t bit_off = mark_at % (uint64_t)(bs * 8);
            uint64_t in_this = (uint64_t)(bs * 8) - bit_off;
            if (in_this > to_mark) in_this = to_mark;
            uint64_t map_idx = fs->sb.free_map_start + mb;
            result_t rrb = objfs_block_read(fs->bc, map_idx, buf);
            if (!result_is_ok(rrb)) { kfree(buf); return rrb; }
            for (uint64_t i = 0; i < in_this; i++) {
              set_bit(buf, bit_off + i, false);
            }
            result_t rwb = objfs_block_write(fs->bc, map_idx, buf);
            if (!result_is_ok(rwb)) { kfree(buf); return rwb; }
            mark_at += in_this;
            to_mark -= in_this;
          }
          kfree(buf);
          *out_first = run_start;
          return RESULT_SUCCESS(0);
        }
      } else {
        run_len = 0;
      }
    }
  }
  kfree(buf);
  return RESULT_FAILURE(RESULT_NOT_FOUND);
}

result_t objfs_free_run(uint64_t first, uint32_t count) {
  objfs_fs_t *fs = objfs_global();
  if (!fs || count == 0)
    return RESULT_FAILURE(RESULT_INVALID);
  uint32_t bs = fs->sb.block_size;
  uint8_t *buf = (uint8_t *)kalloc(bs);
  if (!buf) return RESULT_FAILURE(RESULT_NOMEM);
  uint64_t to_mark = count;
  uint64_t mark_at = first;
  while (to_mark > 0) {
    uint64_t mb = (mark_at / (uint64_t)(bs * 8));
    uint64_t bit_off = mark_at % (uint64_t)(bs * 8);
    uint64_t in_this = (uint64_t)(bs * 8) - bit_off;
    if (in_this > to_mark) in_this = to_mark;
    uint64_t map_idx = fs->sb.free_map_start + mb;
    result_t rrb = objfs_block_read(fs->bc, map_idx, buf);
    if (!result_is_ok(rrb)) { kfree(buf); return rrb; }
    for (uint64_t i = 0; i < in_this; i++) {
      set_bit(buf, bit_off + i, true); // free
    }
    result_t rwb = objfs_block_write(fs->bc, map_idx, buf);
    if (!result_is_ok(rwb)) { kfree(buf); return rwb; }
    mark_at += in_this;
    to_mark -= in_this;
  }
  kfree(buf);
  return RESULT_SUCCESS(0);
}


