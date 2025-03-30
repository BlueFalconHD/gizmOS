#include "physical_alloc.h"

#include "lib/panic.h"
#include "limine.h"
#include "limine_requests.h"
#include <stddef.h>
#include <stdint.h>

static uint64_t free_pages[MAX_PAGES];
static uint64_t free_pages_count = 0;

void initialize_pages(struct limine_memmap_entry **entries,
                      uint64_t entry_count) {

  free_pages_count = 0;

  for (uint64_t i = 0; i < entry_count; i++) {
    struct limine_memmap_entry *entry = entries[i];

    if ((entry->type == LIMINE_MEMMAP_USABLE ||
         entry->type == LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE) &&
        entry->length >= PAGE_SIZE) {

      uint64_t start = (entry->base + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
      uint64_t end = entry->base + entry->length;
      uint64_t pages = (end - start) / PAGE_SIZE;

      for (uint64_t j = 0; j < pages; j++) {
        if (free_pages_count >= MAX_PAGES) {
          panic_msg("Not enough free_pages entries allocated");
        }

        uint64_t page_addr = start + j * PAGE_SIZE;
        free_pages[free_pages_count++] = page_addr;
      }
    }
  }
}

uint64_t get_free_page_count() { return free_pages_count; }

void *alloc_page() {
  if (free_pages_count == 0) {
    panic_msg("Out of memory");
    return NULL;
  }
  uint64_t page_addr = free_pages[--free_pages_count];
  return (void *)(page_addr + hhdm_offset);
}

void free_page(void *ptr) {
  if (free_pages_count >= MAX_PAGES) {
    panic_msg("Free pages list overflow");
  }
  uint64_t addr = (uint64_t)ptr;
  free_pages[free_pages_count++] = addr;
}
