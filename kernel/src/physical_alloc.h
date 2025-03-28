#ifndef PHYSICAL_ALLOC_H
#define PHYSICAL_ALLOC_H

#include "limine.h"
#include <stdint.h>

#define PAGE_SIZE 4096
#define MAX_PAGES (4096 * 1024)

void initialize_pages(struct limine_memmap_entry **entries,
                      uint64_t entry_count);

uint64_t get_free_page_count();
void *alloc_page();
void free_page(void *ptr);

#endif /* PHYSICAL_ALLOC_H */
