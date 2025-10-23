#pragma once

#include <lib/spinlock.h>
#include <lib/types.h>
#include <device/disk.h>

#define BCACHE_BLOCK_SIZE 4096

typedef struct bcache_buf {
  struct bcache_buf *prev;
  struct bcache_buf *next;
  uint64_t blkno;      /* block number within device (in 4K units) */
  g_bool   valid;      /* buffer has valid data */
  g_bool   dirty;      /* needs writeback */
  uint32_t refcnt;
  struct spinlock lock;
  uint8_t  data[BCACHE_BLOCK_SIZE];
} bcache_buf_t;

typedef struct bcache {
  struct spinlock lock;
  disk_t *disk;
  bcache_buf_t *head; /* MRU */
  bcache_buf_t *tail; /* LRU */
  uint32_t capacity;
  uint32_t size;
} bcache_t;

bcache_t *bcache_create(disk_t *disk, uint32_t capacity);
void bcache_destroy(bcache_t *bc);

/* get a buffer for blkno, reading from disk if needed */
bcache_buf_t *bcache_get(bcache_t *bc, uint64_t blkno);
/* mark buffer dirty (caller must hold buf->lock) */
void bcache_mark_dirty(bcache_buf_t *b);
/* release a reference; may write back on eviction */
void bcache_put(bcache_t *bc, bcache_buf_t *b);

/* convenience read/write whole block */
g_bool bcache_read_block(bcache_t *bc, uint64_t blkno, void *out);
g_bool bcache_write_block(bcache_t *bc, uint64_t blkno, const void *in);


