#ifndef PHYSICAL_ALLOC_H
#define PHYSICAL_ALLOC_H

#include <limine.h>
#include <stdint.h>
#include <stdbool.h>

#define PAGE_SIZE 4096
#define MAX_PAGES (4096 * 1024)
#define MAX_ORDER 11  // Support up to 2^11 = 2048 pages (8MB) in a single allocation

void initialize_pages(struct limine_memmap_entry **entries,
                      uint64_t entry_count);

uint64_t get_free_page_count();
void *alloc_page();
void free_page(void *ptr);

// Buddy allocator functions
void *alloc_contiguous_pages(uint64_t num_pages);
void free_contiguous_pages(void *ptr, uint64_t num_pages);
uint64_t get_largest_free_block_pages();

// Helper functions
uint64_t bytes_to_pages(uint64_t bytes);
uint64_t pages_to_bytes(uint64_t pages);

// Debug functions
void buddy_dump_state();

#endif /* PHYSICAL_ALLOC_H */
