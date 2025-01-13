#include "physical_alloc.h"
#include "device/term.h"

#include <stddef.h>
#include <stdint.h>
#include "limine.h"
#include "string.h"
#include "hhdm.h"

static struct page_header *head_page;

void initialize_pages(struct limine_memmap_entry **entries, uint64_t entry_count) {

    head_page = NULL;
    struct page_header *prev = NULL;

    char buffer[64];

    for (uint64_t i = 0; i < entry_count; i++) {
        struct limine_memmap_entry *entry = entries[i];
        if (entry->type == LIMINE_MEMMAP_USABLE && entry->length >= PAGE_SIZE) { // TODO: figure out why adding || entry->type == LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE breaks things
            // Align the start address to page boundary
            uint64_t start = (entry->base + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
            uint64_t end = entry->base + entry->length;
            uint64_t pages = (end - start) / PAGE_SIZE;

            // Debug print
            term_puts("usable region  ");
            itoa_hex(start + hhdm_offset, buffer);
            term_puts(buffer);
            term_puts(" -> ");
            itoa_hex(end + hhdm_offset, buffer);
            term_puts(buffer);
            term_puts(" (fits ");
            uint64_to_str(pages, buffer);
            term_puts(buffer);
            term_puts(" pages)\n");

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
