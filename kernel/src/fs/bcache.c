#include "bcache.h"
#include <lib/kalloc.h>
#include <lib/memory.h>

static void lru_move_front(bcache_t *bc, bcache_buf_t *b) {
  if (bc->head == b) return;
  /* unlink */
  if (b->prev) b->prev->next = b->next;
  if (b->next) b->next->prev = b->prev;
  if (bc->tail == b) bc->tail = b->prev;
  /* insert at head */
  b->prev = NULL;
  b->next = bc->head;
  if (bc->head) bc->head->prev = b;
  bc->head = b;
  if (!bc->tail) bc->tail = b;
}

static void lru_insert_front(bcache_t *bc, bcache_buf_t *b) {
  b->prev = NULL;
  b->next = bc->head;
  if (bc->head) bc->head->prev = b;
  bc->head = b;
  if (!bc->tail) bc->tail = b;
}

bcache_t *bcache_create(disk_t *disk, uint32_t capacity) {
  if (capacity == 0) capacity = 64;
  bcache_t *bc = (bcache_t *)kalloc(sizeof(bcache_t));
  if (!bc) return NULL;
  memset(bc, 0, sizeof(*bc));
  initlock(&bc->lock, "bcache");
  bc->disk = disk;
  bc->capacity = capacity;
  bc->size = 0;
  bc->head = bc->tail = NULL;
  return bc;
}

void bcache_destroy(bcache_t *bc) {
  if (!bc) return;
  bcache_buf_t *b = bc->head;
  while (b) {
    bcache_buf_t *n = b->next;
    kfree(b);
    b = n;
  }
  kfree(bc);
}

static bcache_buf_t *alloc_buf(uint64_t blkno) {
  bcache_buf_t *b = (bcache_buf_t *)kalloc(sizeof(bcache_buf_t));
  if (!b) return NULL;
  memset(b, 0, sizeof(*b));
  initlock(&b->lock, "bcbuf");
  b->blkno = blkno;
  return b;
}

bcache_buf_t *bcache_get(bcache_t *bc, uint64_t blkno) {
  if (!bc) return NULL;
  acquire(&bc->lock);
  /* find existing */
  for (bcache_buf_t *it = bc->head; it; it = it->next) {
    if (it->blkno == blkno) {
      it->refcnt++;
      lru_move_front(bc, it);
      release(&bc->lock);
      acquire(&it->lock);
      if (!it->valid) {
        /* read from disk */
        release(&it->lock);
        /* convert 4K blockno to 512B sectors */
        uint64_t sector = blkno * (BCACHE_BLOCK_SIZE / 512);
        if (!disk_read(bc->disk, sector, it->data, (BCACHE_BLOCK_SIZE / 512))) {
          acquire(&it->lock);
          it->valid = 0;
          release(&it->lock);
          return NULL;
        }
        acquire(&it->lock);
        it->valid = 1;
      }
      release(&it->lock);
      return it;
    }
  }

  /* need new */
  bcache_buf_t *b = alloc_buf(blkno);
  if (!b) { release(&bc->lock); return NULL; }
  b->refcnt = 1;

  /* maybe evict from tail if over capacity */
  if (bc->size >= bc->capacity) {
    /* find LRU with refcnt==0 */
    bcache_buf_t *ev = bc->tail;
    while (ev && ev->refcnt != 0) ev = ev->prev;
    if (ev) {
      /* unlink ev */
      if (ev->prev) ev->prev->next = ev->next;
      if (ev->next) ev->next->prev = ev->prev;
      if (bc->head == ev) bc->head = ev->next;
      if (bc->tail == ev) bc->tail = ev->prev;
      /* writeback if dirty */
      if (ev->dirty) {
        uint64_t sector = ev->blkno * (BCACHE_BLOCK_SIZE / 512);
        disk_write(bc->disk, sector, ev->data, (BCACHE_BLOCK_SIZE / 512));
      }
      kfree(ev);
      bc->size--;
    }
  }

  /* insert new at head */
  lru_insert_front(bc, b);
  bc->size++;
  release(&bc->lock);

  /* read from disk */
  uint64_t sector = blkno * (BCACHE_BLOCK_SIZE / 512);
  if (!disk_read(bc->disk, sector, b->data, (BCACHE_BLOCK_SIZE / 512))) {
    acquire(&bc->lock);
    /* drop from list */
    if (b->prev) b->prev->next = b->next;
    if (b->next) b->next->prev = b->prev;
    if (bc->head == b) bc->head = b->next;
    if (bc->tail == b) bc->tail = b->prev;
    bc->size--;
    release(&bc->lock);
    kfree(b);
    return NULL;
  }

  acquire(&b->lock);
  b->valid = 1;
  release(&b->lock);
  return b;
}

void bcache_mark_dirty(bcache_buf_t *b) {
  if (!b) return;
  b->dirty = 1;
}

void bcache_put(bcache_t *bc, bcache_buf_t *b) {
  if (!bc || !b) return;
  acquire(&bc->lock);
  if (b->refcnt > 0) b->refcnt--;
  /* move towards MRU */
  lru_move_front(bc, b);
  release(&bc->lock);
}

g_bool bcache_read_block(bcache_t *bc, uint64_t blkno, void *out) {
  bcache_buf_t *b = bcache_get(bc, blkno);
  if (!b) return false;
  memcpy(out, b->data, BCACHE_BLOCK_SIZE);
  bcache_put(bc, b);
  return true;
}

g_bool bcache_write_block(bcache_t *bc, uint64_t blkno, const void *in) {
  bcache_buf_t *b = bcache_get(bc, blkno);
  if (!b) return false;
  memcpy(b->data, in, BCACHE_BLOCK_SIZE);
  b->valid = 1;
  b->dirty = 1;
  bcache_put(bc, b);
  return true;
}


