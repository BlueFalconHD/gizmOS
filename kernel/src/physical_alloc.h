#ifndef PHYSICAL_ALLOC_H
#define PHYSICAL_ALLOC_H

#include "memory.h"
#include "memory_map.h"
#include "limine.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define PAGE_SIZE 4096
#define MAX_PAGES 4096 * 16

struct page_header {
    bool free;
    struct page_header *next;
};

#define PAGE_HEADER_SIZE (sizeof(struct page_header))

extern struct page_header *head_page;

void initialize_pages(struct limine_memmap_entry **entries, uint64_t entry_count);

uint64_t get_free_page_count();
void *alloc_page();
void free_page(void *ptr);

#endif /* PHYSICAL_ALLOC_H */
