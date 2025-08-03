#ifndef BUDDY_ALLOCATOR_H
#define BUDDY_ALLOCATOR_H

#include <limine.h>
#include <stdint.h>

/**
 * Buddy Allocator - Fast O(log n) allocation and deallocation
 *
 * Supports allocation orders 0-10:
 * - Order 0: 4KB (1 page)
 * - Order 1: 8KB (2 pages)
 * - Order 2: 16KB (4 pages)
 * - ...
 * - Order 10: 1MB (256 pages)
 */

// Initialize buddy allocator with memory map
void buddy_allocator_init(struct limine_memmap_entry **entries,
                          uint64_t entry_count);

// Allocate 2^order pages (returns virtual address in HHDM)
void *buddy_alloc_pages(int order);

// Free 2^order pages (takes virtual address in HHDM)
void buddy_free_pages(void *ptr, int order);

void *buddy_alloc_page(void);    // Allocates 1 page (order 0)
void buddy_free_page(void *ptr); // Frees 1 page (order 0)
uint64_t buddy_get_free_page_count(void);
uint64_t buddy_get_allocated_page_count(void);

// Debug and statistics
void buddy_print_stats(void);

#endif /* BUDDY_ALLOCATOR_H */
