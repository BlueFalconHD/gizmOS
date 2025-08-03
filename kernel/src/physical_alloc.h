#ifndef PHYSICAL_ALLOC_H
#define PHYSICAL_ALLOC_H

#include "buddy_allocator.h"
#include "lib/macros.h"
#include <limine.h>
#include <stdint.h>

#define PAGE_SIZE 4096
#define MAX_PAGES (4096 * 512)

// void initialize_pages(struct limine_memmap_entry **entries,
//                       uint64_t entry_count);

// uint64_t get_free_page_count();
// void *alloc_page();
// void free_page(void *ptr);

G_INLINE uint64_t get_free_page_count() { return buddy_get_free_page_count(); }

G_INLINE void *alloc_page() { return buddy_alloc_page(); }

G_INLINE void free_page(void *ptr) { buddy_free_page(ptr); }

#endif /* PHYSICAL_ALLOC_H */
