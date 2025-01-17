#include "physical_alloc.h"

#include <stddef.h>
#include <stdint.h>
#include "limine.h"
#include <lib/str.h>
#include "hhdm.h"

struct page_header *head_page = NULL;

void initialize_pages(struct limine_memmap_entry **entries, uint64_t entry_count) {

    head_page = NULL;
    struct page_header *prev = NULL;


    for (uint64_t i = 0; i < entry_count; i++) {
        struct limine_memmap_entry *entry = entries[i];
        if (entry->type == LIMINE_MEMMAP_USABLE && entry->length >= PAGE_SIZE) { // TODO: figure out why adding || entry->type == LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE breaks things
            // Align the start address to page boundary
            uint64_t start = (entry->base + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
            uint64_t end = entry->base + entry->length;
            uint64_t pages = (end - start) / PAGE_SIZE;


            for (uint64_t j = 0; j < pages; j++) {
                // Add HHDM offset to access physical memory
                struct page_header *page = (struct page_header *)((start + j * PAGE_SIZE) + hhdm_offset);
                page->free = true;
                page->next = NULL;

                if (prev != NULL) {
                    prev->next = page;
                }
                prev = page;
                if (head_page == NULL) {
                    head_page = page;
                }
            }
        }
    }
}

uint64_t get_free_page_count() {

    uint64_t count = 0;
    struct page_header *curr = head_page;
    while (curr != NULL) {
        if (curr->free) {
            count++;
        }
        curr = curr->next;
    }
    return count;
}

void *alloc_page() {
    struct page_header *curr = head_page;
    while (curr != NULL) {
        if (curr->free) {
            curr->free = false;
            // Return pointer after the page header
            return (void *)((uintptr_t)curr + PAGE_HEADER_SIZE);
        }
        curr = curr->next;
    }
    return NULL;
}

void free_page(void *ptr) {
    // Adjust pointer back to include the page header
    struct page_header *page = (struct page_header *)((uintptr_t)ptr - PAGE_HEADER_SIZE);
    page->free = true;
}
