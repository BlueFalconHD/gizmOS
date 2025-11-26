#include "libc.h"
#include "../sys/syscall.h"
#include <stdint.h>

void *memcpy(void *dst, const void *src, size_t n) {
  unsigned char *d = (unsigned char *)dst;
  const unsigned char *s = (const unsigned char *)src;
  for (size_t i = 0; i < n; i++) d[i] = s[i];
  return dst;
}

void *memset(void *dst, int c, size_t n) {
  unsigned char *d = (unsigned char *)dst;
  for (size_t i = 0; i < n; i++) d[i] = (unsigned char)c;
  return dst;
}

size_t strlen(const char *s) {
  size_t i = 0; while (s[i]) i++; return i;
}

typedef struct mem_chunk {
  size_t size;
  int free;
  struct mem_chunk *next;
} mem_chunk_t;

static mem_chunk_t *chunk_list = NULL;

static size_t align_size(size_t n) {
  const size_t align = sizeof(void *) * 2;
  return (n + (align - 1)) & ~(align - 1);
}

static void split_chunk(mem_chunk_t *chunk, size_t size) {
  size_t excess = chunk->size > size ? chunk->size - size : 0;
  if (excess <= sizeof(mem_chunk_t) + 16)
    return;
  mem_chunk_t *next =
      (mem_chunk_t *)((unsigned char *)(chunk + 1) + size);
  next->size = excess - sizeof(mem_chunk_t);
  next->free = 1;
  next->next = chunk->next;
  chunk->size = size;
  chunk->next = next;
}

static mem_chunk_t *find_fit(size_t size) {
  mem_chunk_t *curr = chunk_list;
  while (curr) {
    if (curr->free && curr->size >= size) {
      split_chunk(curr, size);
      curr->free = 0;
      return curr;
    }
    curr = curr->next;
  }
  return NULL;
}

static mem_chunk_t *extend_heap(size_t size) {
  size_t total = sizeof(mem_chunk_t) + size;
  mem_chunk_t *chunk = (mem_chunk_t *)sys_sbrk((long)total);
  if (chunk == (void *)-1)
    return NULL;
  chunk->size = size;
  chunk->free = 0;
  chunk->next = NULL;
  if (!chunk_list) {
    chunk_list = chunk;
  } else {
    mem_chunk_t *tail = chunk_list;
    while (tail->next)
      tail = tail->next;
    tail->next = chunk;
  }
  return chunk;
}

static void coalesce_blocks() {
  mem_chunk_t *curr = chunk_list;
  while (curr && curr->next) {
    mem_chunk_t *next = curr->next;
    unsigned char *curr_end =
        (unsigned char *)(curr + 1) + curr->size;
    if (curr->free && next->free &&
        curr_end == (unsigned char *)next) {
      curr->size += sizeof(mem_chunk_t) + next->size;
      curr->next = next->next;
      continue;
    }
    curr = curr->next;
  }
}

void *malloc(size_t size) {
  if (size == 0)
    return NULL;
  size = align_size(size);
  mem_chunk_t *chunk = find_fit(size);
  if (!chunk) {
    chunk = extend_heap(size);
    if (!chunk)
      return NULL;
  }
  return (void *)(chunk + 1);
}

static mem_chunk_t *ptr_to_chunk(void *ptr) {
  if (!ptr)
    return NULL;
  return ((mem_chunk_t *)ptr) - 1;
}

void free(void *ptr) {
  mem_chunk_t *chunk = ptr_to_chunk(ptr);
  if (!chunk)
    return;
  chunk->free = 1;
  coalesce_blocks();
}

void *calloc(size_t nmemb, size_t size) {
  if (nmemb == 0 || size == 0)
    return NULL;
  if (nmemb > SIZE_MAX / size)
    return NULL;
  size_t total = nmemb * size;
  void *ptr = malloc(total);
  if (ptr)
    memset(ptr, 0, total);
  return ptr;
}

void *realloc(void *ptr, size_t size) {
  if (!ptr)
    return malloc(size);
  if (size == 0) {
    free(ptr);
    return NULL;
  }
  size = align_size(size);
  mem_chunk_t *chunk = ptr_to_chunk(ptr);
  if (!chunk)
    return NULL;
  if (chunk->size >= size)
    return ptr;
  void *new_ptr = malloc(size);
  if (!new_ptr)
    return NULL;
  size_t copy = chunk->size < size ? chunk->size : size;
  memcpy(new_ptr, ptr, copy);
  free(ptr);
  return new_ptr;
}


