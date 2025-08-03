#include "buddy_allocator.h"
#include "lib/panic.h"
#include "lib/print.h"
#include "lib/str.h"
#include "limine_requests.h"
#include <stddef.h>
#include <stdint.h>

extern char kstart[];
extern char kend[];

// Buddy allocator configuration
#define BUDDY_MIN_ORDER 0  // 4KB minimum block
#define BUDDY_MAX_ORDER 10 // 1MB maximum block
#define BUDDY_NUM_ORDERS (BUDDY_MAX_ORDER + 1)
#define BUDDY_PAGE_SIZE 4096
#define BUDDY_MAX_PAGES (4096 * 1024) // Same as current allocator

// Each free block contains a linked list node
struct buddy_block {
  struct buddy_block *next;
  struct buddy_block *prev;
};

// Free lists for each order
static struct buddy_block *free_lists[BUDDY_NUM_ORDERS];

// Bitmap to track allocated blocks (1 bit per min-size block)
static uint64_t *allocation_bitmap;
static uint64_t bitmap_size_words;

// Memory region managed by buddy allocator
static uint64_t buddy_base_addr; // Physical address of managed region
static uint64_t buddy_total_pages;
static uint64_t buddy_total_size;

// Statistics
static uint64_t buddy_free_pages_count;
static uint64_t buddy_allocated_pages_count;

// Helper macros
#define BUDDY_BLOCK_SIZE(order) (BUDDY_PAGE_SIZE << (order))
#define BUDDY_BLOCKS_PER_ORDER(order) (buddy_total_pages >> (order))
#define BUDDY_VIRT_TO_PHYS(virt) ((uint64_t)(virt) - hhdm_offset)
#define BUDDY_PHYS_TO_VIRT(phys) ((void *)((phys) + hhdm_offset))

// Get block index for given physical address and order
static inline uint64_t buddy_addr_to_index(uint64_t phys_addr, int order) {
  return (phys_addr - buddy_base_addr) >> (12 + order);
}

// Get physical address for given block index and order
static inline uint64_t buddy_index_to_addr(uint64_t index, int order) {
  return buddy_base_addr + (index << (12 + order));
}

// Get buddy index for a given block index
static inline uint64_t buddy_get_buddy_index(uint64_t index, int order) {
  return index ^ (1ULL << order);
}

// Check if a block is allocated in the bitmap
static inline int buddy_is_allocated(uint64_t phys_addr) {
  uint64_t page_index = (phys_addr - buddy_base_addr) / BUDDY_PAGE_SIZE;
  uint64_t word_index = page_index / 64;
  uint64_t bit_index = page_index % 64;

  if (word_index >= bitmap_size_words)
    return 0;
  return (allocation_bitmap[word_index] >> bit_index) & 1;
}

// Mark block as allocated in bitmap
static inline void buddy_mark_allocated(uint64_t phys_addr, int order) {
  uint64_t block_size = BUDDY_BLOCK_SIZE(order);
  uint64_t pages_in_block = block_size / BUDDY_PAGE_SIZE;
  uint64_t start_page = (phys_addr - buddy_base_addr) / BUDDY_PAGE_SIZE;

  for (uint64_t i = 0; i < pages_in_block; i++) {
    uint64_t page_index = start_page + i;
    uint64_t word_index = page_index / 64;
    uint64_t bit_index = page_index % 64;

    if (word_index < bitmap_size_words) {
      allocation_bitmap[word_index] |= (1ULL << bit_index);
    }
  }
}

// Mark block as free in bitmap
static inline void buddy_mark_free(uint64_t phys_addr, int order) {
  uint64_t block_size = BUDDY_BLOCK_SIZE(order);
  uint64_t pages_in_block = block_size / BUDDY_PAGE_SIZE;
  uint64_t start_page = (phys_addr - buddy_base_addr) / BUDDY_PAGE_SIZE;

  for (uint64_t i = 0; i < pages_in_block; i++) {
    uint64_t page_index = start_page + i;
    uint64_t word_index = page_index / 64;
    uint64_t bit_index = page_index % 64;

    if (word_index < bitmap_size_words) {
      allocation_bitmap[word_index] &= ~(1ULL << bit_index);
    }
  }
}

// Check if entire buddy block is free
static inline int buddy_is_buddy_free(uint64_t buddy_addr, int order) {
  uint64_t block_size = BUDDY_BLOCK_SIZE(order);
  uint64_t pages_in_block = block_size / BUDDY_PAGE_SIZE;
  uint64_t start_page = (buddy_addr - buddy_base_addr) / BUDDY_PAGE_SIZE;

  for (uint64_t i = 0; i < pages_in_block; i++) {
    uint64_t page_index = start_page + i;
    uint64_t word_index = page_index / 64;
    uint64_t bit_index = page_index % 64;

    if (word_index >= bitmap_size_words)
      return 0;
    if ((allocation_bitmap[word_index] >> bit_index) & 1)
      return 0;
  }
  return 1;
}

// Remove block from free list
static void buddy_remove_from_free_list(struct buddy_block *block, int order) {
  if (block->prev) {
    block->prev->next = block->next;
  } else {
    free_lists[order] = block->next;
  }

  if (block->next) {
    block->next->prev = block->prev;
  }

  block->next = block->prev = NULL;
}

// Add block to front of free list
static void buddy_add_to_free_list(struct buddy_block *block, int order) {
  block->prev = NULL;
  block->next = free_lists[order];

  if (free_lists[order]) {
    free_lists[order]->prev = block;
  }

  free_lists[order] = block;
}

// Find block in free list (for removal during coalescing)
static struct buddy_block *buddy_find_in_free_list(uint64_t phys_addr,
                                                   int order) {
  void *virt_addr = BUDDY_PHYS_TO_VIRT(phys_addr);
  struct buddy_block *current = free_lists[order];

  while (current) {
    if (current == (struct buddy_block *)virt_addr) {
      return current;
    }
    current = current->next;
  }
  return NULL;
}

void buddy_allocator_init(struct limine_memmap_entry **entries,
                          uint64_t entry_count) {
  // Initialize free lists
  for (int i = 0; i < BUDDY_NUM_ORDERS; i++) {
    free_lists[i] = NULL;
  }

  buddy_free_pages_count = 0;
  buddy_allocated_pages_count = 0;

  // Find largest contiguous usable memory region
  uint64_t best_base = 0;
  uint64_t best_size = 0;

  // Calculate kernel physical range
  uint64_t kernel_phys_start = (uint64_t)kstart - hhdm_offset;
  uint64_t kernel_phys_end = (uint64_t)kend - hhdm_offset;
  kernel_phys_start &= ~(BUDDY_PAGE_SIZE - 1);
  kernel_phys_end =
      (kernel_phys_end + BUDDY_PAGE_SIZE - 1) & ~(BUDDY_PAGE_SIZE - 1);

  // Find best memory region to manage
  for (uint64_t i = 0; i < entry_count; i++) {
    struct limine_memmap_entry *entry = entries[i];

    if (entry->type != LIMINE_MEMMAP_USABLE ||
        entry->length < BUDDY_PAGE_SIZE * 64) {
      continue;
    }

    uint64_t region_start =
        (entry->base + BUDDY_PAGE_SIZE - 1) & ~(BUDDY_PAGE_SIZE - 1);
    uint64_t region_end =
        (entry->base + entry->length) & ~(BUDDY_PAGE_SIZE - 1);
    uint64_t region_size = region_end - region_start;

    // Skip if overlaps with kernel
    if (region_start < kernel_phys_end && region_end > kernel_phys_start) {
      continue;
    }

    if (region_size > best_size) {
      best_base = region_start;
      best_size = region_size;
    }
  }

  if (best_size == 0) {
    panic_msg("buddy_allocator: No suitable memory region found");
  }

  buddy_base_addr = best_base;
  buddy_total_size = best_size;
  buddy_total_pages = best_size / BUDDY_PAGE_SIZE;

  // Allocate bitmap for tracking allocations (use some managed memory)
  bitmap_size_words = (buddy_total_pages + 63) / 64;
  uint64_t bitmap_size_bytes = bitmap_size_words * sizeof(uint64_t);

  // Place bitmap at start of managed region
  allocation_bitmap = (uint64_t *)BUDDY_PHYS_TO_VIRT(buddy_base_addr);

  // Clear bitmap
  for (uint64_t i = 0; i < bitmap_size_words; i++) {
    allocation_bitmap[i] = 0;
  }

  // Mark bitmap pages as allocated
  uint64_t bitmap_pages =
      (bitmap_size_bytes + BUDDY_PAGE_SIZE - 1) / BUDDY_PAGE_SIZE;
  for (uint64_t i = 0; i < bitmap_pages; i++) {
    buddy_mark_allocated(buddy_base_addr + i * BUDDY_PAGE_SIZE,
                         BUDDY_MIN_ORDER);
  }

  // Add remaining memory to free lists, starting from highest order
  uint64_t current_addr = buddy_base_addr + bitmap_pages * BUDDY_PAGE_SIZE;
  uint64_t remaining_size = buddy_total_size - bitmap_pages * BUDDY_PAGE_SIZE;

  while (remaining_size >= BUDDY_PAGE_SIZE) {
    // Find largest block that fits
    int order = BUDDY_MAX_ORDER;
    while (order >= 0 && BUDDY_BLOCK_SIZE(order) > remaining_size) {
      order--;
    }

    if (order < 0)
      break;

    // Align address to block boundary for this order
    uint64_t block_size = BUDDY_BLOCK_SIZE(order);
    uint64_t aligned_addr = (current_addr + block_size - 1) & ~(block_size - 1);

    if (aligned_addr + block_size > buddy_base_addr + buddy_total_size) {
      // Can't fit this block, try smaller
      if (order == 0)
        break;
      order--;
      continue;
    }

    // Add block to free list
    struct buddy_block *block =
        (struct buddy_block *)BUDDY_PHYS_TO_VIRT(aligned_addr);
    buddy_add_to_free_list(block, order);
    buddy_free_pages_count += block_size / BUDDY_PAGE_SIZE;

    current_addr = aligned_addr + block_size;
    remaining_size = (buddy_base_addr + buddy_total_size) - current_addr;
  }
}

void *buddy_alloc_pages(int order) {
  if (order < 0 || order > BUDDY_MAX_ORDER) {
    return NULL;
  }

  // Look for free block of requested order or larger
  int current_order = order;
  while (current_order <= BUDDY_MAX_ORDER && !free_lists[current_order]) {
    current_order++;
  }

  if (current_order > BUDDY_MAX_ORDER) {
    return NULL; // Out of memory
  }

  // Remove block from free list
  struct buddy_block *block = free_lists[current_order];
  buddy_remove_from_free_list(block, current_order);

  uint64_t block_addr = BUDDY_VIRT_TO_PHYS((uint64_t)block);

  // Split larger blocks down to requested order
  while (current_order > order) {
    current_order--;
    uint64_t buddy_addr = block_addr + BUDDY_BLOCK_SIZE(current_order);
    struct buddy_block *buddy =
        (struct buddy_block *)BUDDY_PHYS_TO_VIRT(buddy_addr);
    buddy_add_to_free_list(buddy, current_order);
  }

  // Mark as allocated
  buddy_mark_allocated(block_addr, order);
  buddy_allocated_pages_count += BUDDY_BLOCK_SIZE(order) / BUDDY_PAGE_SIZE;
  buddy_free_pages_count -= BUDDY_BLOCK_SIZE(order) / BUDDY_PAGE_SIZE;

  return block;
}

void buddy_free_pages(void *ptr, int order) {
  if (!ptr || order < 0 || order > BUDDY_MAX_ORDER) {
    return;
  }

  uint64_t block_addr = BUDDY_VIRT_TO_PHYS((uint64_t)ptr);

  // Mark as free
  buddy_mark_free(block_addr, order);
  buddy_allocated_pages_count -= BUDDY_BLOCK_SIZE(order) / BUDDY_PAGE_SIZE;
  buddy_free_pages_count += BUDDY_BLOCK_SIZE(order) / BUDDY_PAGE_SIZE;

  // Coalesce with buddy if possible
  int current_order = order;
  uint64_t current_addr = block_addr;

  while (current_order < BUDDY_MAX_ORDER) {
    uint64_t block_index = buddy_addr_to_index(current_addr, current_order);
    uint64_t buddy_index = buddy_get_buddy_index(block_index, current_order);
    uint64_t buddy_addr = buddy_index_to_addr(buddy_index, current_order);

    // Check if buddy is free
    if (!buddy_is_buddy_free(buddy_addr, current_order)) {
      break;
    }

    // Remove buddy from free list
    struct buddy_block *buddy_block =
        buddy_find_in_free_list(buddy_addr, current_order);
    if (buddy_block) {
      buddy_remove_from_free_list(buddy_block, current_order);
    }

    // Merge with buddy (current block becomes the lower address)
    if (buddy_addr < current_addr) {
      current_addr = buddy_addr;
    }

    current_order++;
  }

  // Add merged block to appropriate free list
  struct buddy_block *final_block =
      (struct buddy_block *)BUDDY_PHYS_TO_VIRT(current_addr);
  buddy_add_to_free_list(final_block, current_order);
}

// Compatibility functions for existing allocator interface
void *buddy_alloc_page(void) { return buddy_alloc_pages(BUDDY_MIN_ORDER); }

void buddy_free_page(void *ptr) { buddy_free_pages(ptr, BUDDY_MIN_ORDER); }

uint64_t buddy_get_free_page_count(void) { return buddy_free_pages_count; }

uint64_t buddy_get_allocated_page_count(void) {
  return buddy_allocated_pages_count;
}

// Debug function to print allocator state
void buddy_print_stats(void) {
  print("Buddy Allocator Stats:\n", PRINT_FLAG_BOTH);

  char buffer[128];
  print("  Total pages: ", PRINT_FLAG_BOTH);
  strfuint(buddy_total_pages, buffer);

  print(buffer, PRINT_FLAG_BOTH);
  print("\n", PRINT_FLAG_BOTH);

  print("  Free pages: ", PRINT_FLAG_BOTH);
  strfuint(buddy_free_pages_count, buffer);
  print(buffer, PRINT_FLAG_BOTH);
  print("\n", PRINT_FLAG_BOTH);

  print("  Allocated pages: ", PRINT_FLAG_BOTH);
  strfuint(buddy_allocated_pages_count, buffer);
  print(buffer, PRINT_FLAG_BOTH);
  print("\n", PRINT_FLAG_BOTH);

  for (int order = 0; order <= BUDDY_MAX_ORDER; order++) {
    int count = 0;
    struct buddy_block *current = free_lists[order];
    while (current) {
      count++;
      current = current->next;
    }

    if (count > 0) {
      print("  Order ", PRINT_FLAG_BOTH);
      strfuint(order, buffer);
      print(buffer, PRINT_FLAG_BOTH);
      print(" (", PRINT_FLAG_BOTH);
      strfuint(BUDDY_BLOCK_SIZE(order), buffer);
      print(buffer, PRINT_FLAG_BOTH);
      print(" bytes): ", PRINT_FLAG_BOTH);
      strfuint(count, buffer);
      print(buffer, PRINT_FLAG_BOTH);
      print(" blocks\n", PRINT_FLAG_BOTH);
    }
  }
}
