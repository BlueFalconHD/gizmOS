#include "physical_alloc.h"

#include "lib/panic.h"
#include "lib/print.h"
#include "lib/str.h"
#include "limine.h"
#include "limine_requests.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

extern char kstart[];
extern char kend[];

// Original simple allocator for single pages
static uint64_t free_pages[MAX_PAGES];
static uint64_t free_pages_count = 0;

// Buddy allocator data structures
struct buddy_free_list {
    uint64_t pages[256];  // Store page indices
    uint32_t count;
};

static struct buddy_free_list buddy_lists[MAX_ORDER];
static uint8_t *page_orders = NULL;  // Track the order of each allocated block
static uint64_t total_buddy_pages = 0;
static uint64_t buddy_base_page = 0;
static bool buddy_initialized = false;

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

// Buddy allocator helper functions
static inline uint64_t pages_to_order(uint64_t pages) {
    if (pages <= 1) return 0;
    uint64_t order = 0;
    uint64_t size = 1;
    while (size < pages) {
        size <<= 1;
        order++;
    }
    return order;
}

static inline uint64_t order_to_pages(uint64_t order) {
    return 1ULL << order;
}

static inline uint64_t get_buddy_page(uint64_t page, uint64_t order) {
    return page ^ (1ULL << order);
}

// Initialize buddy allocator with a contiguous range of pages
static void init_buddy_allocator() {
    if (buddy_initialized) return;
    
    // Find a large contiguous range in our free pages for buddy allocation
    // For simplicity, we'll use the first 1024 pages if available
    if (free_pages_count < 1024) {
        return; // Not enough pages for buddy allocator
    }
    
    // Use some pages from the end of our free list for buddy allocator
    uint64_t buddy_page_count = 1024;  // Use 1024 pages (4MB) for buddy allocator
    if (buddy_page_count > free_pages_count / 2) {
        buddy_page_count = free_pages_count / 2;
    }
    
    // Round down to nearest power of 2
    uint64_t power = 1;
    while (power * 2 <= buddy_page_count) {
        power *= 2;
    }
    buddy_page_count = power;
    
    total_buddy_pages = buddy_page_count;
    
    // Allocate metadata using some of our simple allocator pages
    uint64_t metadata_pages = (buddy_page_count + PAGE_SIZE - 1) / PAGE_SIZE;
    if (metadata_pages > free_pages_count - buddy_page_count) {
        return; // Not enough pages for metadata
    }
    
    // Get metadata page
    void *metadata_ptr = alloc_page();
    if (!metadata_ptr) return;
    
    page_orders = (uint8_t *)metadata_ptr;
    
    // Initialize buddy free lists
    for (int i = 0; i < MAX_ORDER; i++) {
        buddy_lists[i].count = 0;
    }
    
    // Set up the buddy allocator with one large block
    uint64_t max_order = 0;
    uint64_t size = buddy_page_count;
    while (size > 1) {
        size >>= 1;
        max_order++;
    }
    
    // Remove pages from simple allocator for buddy use
    if (free_pages_count >= buddy_page_count) {
        free_pages_count -= buddy_page_count;
        buddy_base_page = 0; // Will use indices relative to our management
        
        // Add the whole block to the highest order
        if (max_order < MAX_ORDER && buddy_lists[max_order].count < 256) {
            buddy_lists[max_order].pages[buddy_lists[max_order].count++] = 0;
        }
        
        buddy_initialized = true;
    }
}

void *alloc_contiguous_pages(uint64_t num_pages) {
    if (num_pages == 0) return NULL;
    if (num_pages == 1) return alloc_page();
    
    if (!buddy_initialized) {
        init_buddy_allocator();
        if (!buddy_initialized) {
            return NULL; // Buddy allocator couldn't be initialized
        }
    }
    
    uint64_t order = pages_to_order(num_pages);
    if (order >= MAX_ORDER) return NULL;
    
    // Find a free block of the required order or larger
    uint64_t alloc_order = order;
    while (alloc_order < MAX_ORDER && buddy_lists[alloc_order].count == 0) {
        alloc_order++;
    }
    
    if (alloc_order >= MAX_ORDER) return NULL;
    
    // Take a block from the free list
    uint64_t page_idx = buddy_lists[alloc_order].pages[--buddy_lists[alloc_order].count];
    
    // Split the block down to the required order
    while (alloc_order > order) {
        alloc_order--;
        uint64_t buddy_idx = page_idx + (1ULL << alloc_order);
        
        // Add the buddy to the smaller order free list
        if (buddy_lists[alloc_order].count < 256) {
            buddy_lists[alloc_order].pages[buddy_lists[alloc_order].count++] = buddy_idx;
        }
    }
    
    // Mark the order for this allocation
    if (page_orders) {
        page_orders[page_idx] = order;
    }
    
    // Convert page index to actual address
    // For simplicity, we'll allocate from our simple allocator
    // This is a hybrid approach for now
    uint64_t pages_needed = order_to_pages(order);
    if (free_pages_count >= pages_needed) {
        uint64_t first_page = free_pages[free_pages_count - pages_needed];
        free_pages_count -= pages_needed;
        return (void *)(first_page + hhdm_offset);
    }
    
    return NULL;
}

void free_contiguous_pages(void *ptr, uint64_t num_pages) {
    if (!ptr || num_pages == 0) return;
    if (num_pages == 1) {
        free_page(ptr);
        return;
    }
    
    if (!buddy_initialized) {
        // Fall back to simple free for individual pages
        for (uint64_t i = 0; i < num_pages; i++) {
            free_page((void *)((uint64_t)ptr + i * PAGE_SIZE));
        }
        return;
    }
    
    // For now, fall back to simple allocator
    // A full implementation would track which addresses came from buddy allocator
    uint64_t virt = (uint64_t)ptr;
    if (virt < hhdm_offset) return;
    
    uint64_t phys = virt - hhdm_offset;
    for (uint64_t i = 0; i < num_pages; i++) {
        if (free_pages_count < MAX_PAGES) {
            free_pages[free_pages_count++] = phys + (i * PAGE_SIZE);
        }
    }
}

uint64_t get_largest_free_block_pages() {
    uint64_t largest = 1; // At least single pages from simple allocator
    
    if (buddy_initialized) {
        for (int order = MAX_ORDER - 1; order >= 0; order--) {
            if (buddy_lists[order].count > 0) {
                uint64_t block_size = order_to_pages(order);
                if (block_size > largest) {
                    largest = block_size;
                }
                break;
            }
        }
    }
    
    return largest;
}

uint64_t bytes_to_pages(uint64_t bytes) {
    return (bytes + PAGE_SIZE - 1) / PAGE_SIZE;
}

uint64_t pages_to_bytes(uint64_t pages) {
    return pages * PAGE_SIZE;
}

void buddy_dump_state() {
    printf("=== Physical Allocator State ===\n", PRINT_FLAG_BOTH);
    printf("Simple allocator: %{type: int} free pages\n", PRINT_FLAG_BOTH, free_pages_count);
    
    if (buddy_initialized) {
        printf("Buddy allocator initialized: %{type: int} total pages\n", PRINT_FLAG_BOTH, total_buddy_pages);
        
        for (int order = 0; order < MAX_ORDER; order++) {
            if (buddy_lists[order].count > 0) {
                uint64_t block_size = order_to_pages(order);
                printf("Order %{type: int} (%{type: int} pages): %{type: int} blocks\n", 
                       PRINT_FLAG_BOTH, order, block_size, buddy_lists[order].count);
            }
        }
    } else {
        printf("Buddy allocator not initialized\n", PRINT_FLAG_BOTH);
    }
    printf("Largest free block: %{type: int} pages\n", PRINT_FLAG_BOTH, get_largest_free_block_pages());
}
