#include "physical_alloc.h"

#ifndef NEW_ALLOC

#include "lib/panic.h"
#include "lib/print.h"
#include "lib/str.h"
#include "limine.h"
#include "limine_requests.h"
#include <stddef.h>
#include <stdint.h>

extern char kstart[];
extern char kend[];

static uint64_t free_pages[MAX_PAGES];
static uint64_t free_pages_count = 0;

void initialize_pages(struct limine_memmap_entry **entries,
                      uint64_t entry_count) {

  free_pages_count = 0;

  // Calculate the physical memory range occupied by the kernel image.
  // kstart and kend are virtual addresses within the HHDM.
  uint64_t kernel_phys_start = (uint64_t)kstart - hhdm_offset;
  uint64_t kernel_phys_end = (uint64_t)kend - hhdm_offset;

  // Align the kernel physical range to page boundaries for easier comparison.
  kernel_phys_start &= ~(PAGE_SIZE - 1); // Floor to page boundary
  kernel_phys_end = (kernel_phys_end + PAGE_SIZE - 1) &
                    ~(PAGE_SIZE - 1); // Ceil to page boundary

  // Debug print kernel physical range
  // printf("Kernel physical range: 0x%{type: hex} - 0x%{type: hex}\n",
  // PRINT_FLAG_BOTH, kernel_phys_start, kernel_phys_end);

  for (uint64_t i = 0; i < entry_count; i++) {
    struct limine_memmap_entry *entry = entries[i];

    // We only care about genuinely free RAM that's large enough for at least
    // one page.
    if (entry->type != LIMINE_MEMMAP_USABLE || entry->length < PAGE_SIZE) {
      continue;
    }

    // Align the usable region to whole pages.
    uint64_t region_start =
        (entry->base + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1); // Ceil start
    uint64_t region_end =
        entry->base + entry->length; // End address (exclusive)
    region_end &=
        ~(PAGE_SIZE -
          1); // Floor end to ensure we don't go past the usable boundary

    // Iterate through each page in the usable region.
    for (uint64_t page_addr = region_start; page_addr < region_end;
         page_addr += PAGE_SIZE) {

      // Check if this page overlaps with the kernel's physical memory range.
      // A page overlaps if its start is before the kernel end AND its end is
      // after the kernel start.
      uint64_t page_end = page_addr + PAGE_SIZE;
      if (page_addr < kernel_phys_end && page_end > kernel_phys_start) {
        // This page is occupied by the kernel, skip it.
        // printf("Skipping kernel page: 0x%{type: hex}\n", PRINT_FLAG_BOTH,
        // page_addr);
        continue;
      }

      // Check if we have space in our free_pages array.
      if (free_pages_count >= MAX_PAGES) {
        panic_msg("physical_alloc: free_pages array overflow");
      }

      // Add the physical address of the free page to our list.
      free_pages[free_pages_count++] = page_addr;
    }
  }
  // printf("Initialized physical allocator with %{type: int} free pages.\n",
  // PRINT_FLAG_BOTH, free_pages_count);
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

  uint64_t virt = (uint64_t)ptr;

  // Convert back to physical
  if (virt < hhdm_offset) {
    panic_msg_no_cr("free_page: address is not in HHDM range: ");
    char buffer[128];
    hexstrfuint(virt, buffer);
    print(buffer, PRINT_FLAG_BOTH);
    print("\n", PRINT_FLAG_BOTH);
    return;
  }
  uint64_t phys = virt - hhdm_offset;

  free_pages[free_pages_count++] = phys;
}
#endif
