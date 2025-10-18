#include "kalloc.h"
#include "buddy_allocator.h"

#include "lib/debug.h"
#include <lib/log.h>

static inline log_t *kalloc_log() {
  static log_t *l = NULL;
  if (!l) {
    l = g_log_create("mm", "kalloc");
    #if KALLOC_DEBUG
    g_log_set_level(l, LOG_LEVEL_DEBUG);
    #else
    g_log_set_level(l, LOG_LEVEL_INFO);
    #endif
  }
  return l;
}

#include <lib/memory.h>
#include <stddef.h>
#include <stdint.h>

allocation_t recent_allocations[100];
static int allocation_index = 0;

typedef struct kalloc_header {
  uint32_t magic;
  uint32_t order;
  size_t requested_size;
} kalloc_header_t;

#define KALLOC_HEADER_MAGIC 0xBADC0FFE

static inline size_t kalloc_header_size(void) {
  size_t sz = sizeof(kalloc_header_t);
  return (sz + 15) & ~(size_t)15;
}

static int size_to_order(size_t size) {
  if (size == 0) {
    dbg("size == 0");
    return 0;
  }

  size_t page_size = 4096;
  size_t pages_needed = (size + page_size - 1) / page_size;

  if (pages_needed <= 1)
    return 0;

  int order = 0;
  size_t capacity = 1;

  while (capacity < pages_needed && order < 10) {
    order++;
    capacity <<= 1; // capacity *= 2
  }

  if (capacity < pages_needed) {
    dbg("capacity < pages_needed");
    return -1;
  }

  return order;
}

static void record_allocation(void *ptr, size_t size, const char *file,
                              int line) {
  allocation_t *alloc = &recent_allocations[allocation_index];

  alloc->ptr = ptr;
  alloc->size = size;

  const char *filename = file;
  const char *slash = file;

  while (*slash) {
    if (*slash == '/' || *slash == '\\') {
      filename = slash + 1;
    }
    slash++;
  }

  size_t name_len = 0;
  while (filename[name_len] != '\0') {
    name_len++;
  }
  if (name_len >= sizeof(alloc->file)) {
    name_len = sizeof(alloc->file) - 1;
  }

  for (size_t i = 0; i < name_len; i++) {
    alloc->file[i] = filename[i];
  }
  alloc->file[name_len] = '\0';

  int temp_line = line;

  if (line == 0) {
    alloc->line[0] = '0';
    alloc->line[1] = '\0';
  } else {
    char temp[8];
    int temp_len = 0;

    while (temp_line > 0 && temp_len < 7) {
      temp[temp_len++] = '0' + (temp_line % 10);
      temp_line /= 10;
    }

    for (int i = 0; i < temp_len; i++) {
      alloc->line[i] = temp[temp_len - 1 - i];
    }
    alloc->line[temp_len] = '\0';
  }

  allocation_index = (allocation_index + 1) % 100;
}

void *kalloc_impl(size_t size) {
  if (size == 0) {
    dbg("size == 0");
    return NULL;
  }

  size_t header_sz = kalloc_header_size();
  size_t total = size + header_sz;

  int order = size_to_order(total);
  if (order < 0) {
    dbg("size_to_order(...) < 0");
    return NULL;
  }
  void *block = buddy_alloc_pages(order);
  if (!block) {
    dbg("buddy_alloc_pages(...) == NULL");
    return NULL;
  }

  kalloc_header_t *hdr = (kalloc_header_t *)block;
  hdr->magic = KALLOC_HEADER_MAGIC;
  hdr->order = (uint32_t)order;
  hdr->requested_size = size;

  void *user = (void *)((uint8_t *)block + header_sz);
  return user;
}

void kfree_impl(void *ptr) {
  if (ptr == NULL) {
    dbg("ptr == NULL");
    return;
  }

  size_t header_sz = kalloc_header_size();
  kalloc_header_t *hdr = (kalloc_header_t *)((uint8_t *)ptr - header_sz);

  if (hdr->magic != KALLOC_HEADER_MAGIC) {
    LOG_WARN(kalloc_log(), "WARNING: kfree on non-kalloc pointer %{type: hex}", (uint64_t)ptr);
    dbg("hdr->magic != KALLOC_HEADER_MAGIC");
    return;
  }

  void *block = (void *)hdr;
  int order = (int)hdr->order;

  buddy_free_pages(block, order);
}

size_t kalloc_usable_size(void *ptr) {
  if (ptr == NULL) {
    dbg("ptr == NULL");
    return 0;
  }
  size_t header_sz = kalloc_header_size();
  kalloc_header_t *hdr = (kalloc_header_t *)((uint8_t *)ptr - header_sz);
  if (hdr->magic != KALLOC_HEADER_MAGIC) {
    dbg("hdr->magic != KALLOC_HEADER_MAGIC");
    return 0;
  }
  size_t block_size = ((size_t)4096) << hdr->order;
  if (block_size < header_sz) {
    dbg("block_size < header_sz");
    return 0;
  }
  return block_size - header_sz;
}

void *kresize_impl(void *ptr, size_t new_size) {
  if (ptr == NULL) {
    return kalloc_impl(new_size);
  }
  if (new_size == 0) {
    kfree_impl(ptr);
    return NULL;
  }

  size_t header_sz = kalloc_header_size();
  kalloc_header_t *hdr = (kalloc_header_t *)((uint8_t *)ptr - header_sz);
  if (hdr->magic != KALLOC_HEADER_MAGIC) {
    LOG_WARN(kalloc_log(), "WARNING: kresize on non-kalloc pointer %{type: hex}", (uint64_t)ptr);
    dbg("hdr->magic != KALLOC_HEADER_MAGIC");
    return NULL;
  }

  size_t block_size = ((size_t)4096) << hdr->order;
  if (block_size < header_sz) {
    dbg("block_size < header_sz");
    return NULL;
  }
  size_t old_usable = block_size - header_sz;

  if (new_size <= old_usable) {
    hdr->requested_size = new_size;
    return ptr;
  }

  void *new_ptr = kalloc_impl(new_size);
  if (!new_ptr) {
    dbg("kalloc_impl(...) == NULL");
    return NULL;
  }

  size_t to_copy = hdr->requested_size;
  if (to_copy > new_size) {
    to_copy = new_size;
  }
  memcpy(new_ptr, ptr, to_copy);

  kfree_impl(ptr);
  return new_ptr;
}

void *kresize_trace(void *ptr, size_t new_size, const char *file __attribute__((unused)), int line __attribute__((unused))) {
  void *new_ptr = kresize_impl(ptr, new_size);
  if (new_ptr != NULL) {
    record_allocation(new_ptr, new_size, file, line);
#ifdef KALLOC_DEBUG
    LOG_DEBUG(kalloc_log(), "Resized %{type: hex} -> %{type: hex} to %{type: int} bytes (%{type: str}:%{type: int})", (uint64_t)ptr, (uint64_t)new_ptr, (int)new_size, file, line);
#endif
  } else {
    LOG_WARN(kalloc_log(), "Failed to resize %{type: hex} to %{type: int} bytes (%{type: str}:%{type: int})", (uint64_t)ptr, (int)new_size, file, line);
    dbg("kresize_impl(...) == NULL");
  }
  return new_ptr;
}

void *kalloc_trace(size_t size, const char *file, int line) {
  void *ptr = kalloc_impl(size);

  if (ptr != NULL) {
    record_allocation(ptr, size, file, line);

#ifdef KALLOC_DEBUG
    LOG_DEBUG(kalloc_log(), "Allocated %{type: int} bytes at %{type: hex} (%{type: str}:%{type: int})", (int)size, (uint64_t)ptr, file, line);
#endif
  }

  return ptr;
}

void kfree_trace(void *ptr, const char *file __attribute__((unused)), int line __attribute__((unused))) {
  if (ptr != NULL) {
#ifdef KALLOC_DEBUG
    LOG_DEBUG(kalloc_log(), "Freeing %{type: hex} (%{type: str}:%{type: int})", (uint64_t)ptr, file, line);
#endif
  }

  kfree_impl(ptr);
}
