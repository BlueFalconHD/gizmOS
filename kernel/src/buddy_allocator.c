#include "buddy_allocator.h"
#include "lib/fmt.h"
#include "lib/panic.h"
#include "lib/print.h"
#include "lib/str.h"
#include "lib/types.h"
#include "limine_requests.h"
#include "platform/registers.h"
#include <stddef.h>
#include <stdint.h>

// #define BUDDY_ALLOCATOR_DEBUG

extern char kstart[];
extern char kend[];

g_bool is_buddy_allocator_initialized = false;

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
#ifdef BUDDY_ALLOCATOR_DEBUG
  print("buddy_addr_to_index: phys_addr = 0x", PRINT_FLAG_BOTH);
  char buf[20];
  hexstrfuint(phys_addr, buf);
  print(buf, PRINT_FLAG_BOTH);
  print(", order = ", PRINT_FLAG_BOTH);
  strfuint(order, buf);
  print(buf, PRINT_FLAG_BOTH);
  print("\n", PRINT_FLAG_BOTH);
#endif
  return (phys_addr - buddy_base_addr) >> (12 + order);
}

// Get physical address for given block index and order
static inline uint64_t buddy_index_to_addr(uint64_t index, int order) {
#ifdef BUDDY_ALLOCATOR_DEBUG
  print("buddy_index_to_addr: index = ", PRINT_FLAG_BOTH);
  char buf[20];
  strfuint(index, buf);
  print(buf, PRINT_FLAG_BOTH);
  print(", order = ", PRINT_FLAG_BOTH);
  strfuint(order, buf);
  print(buf, PRINT_FLAG_BOTH);
  print("\n", PRINT_FLAG_BOTH);
#endif
  return buddy_base_addr + (index << (12 + order));
}

// Get buddy index for a given block index
static inline uint64_t buddy_get_buddy_index(uint64_t index, int order) {
#ifdef BUDDY_ALLOCATOR_DEBUG
  print("buddy_get_buddy_index: index = ", PRINT_FLAG_BOTH);
  char buf[20];
  strfuint(index, buf);
  print(buf, PRINT_FLAG_BOTH);
  print(", order = ", PRINT_FLAG_BOTH);
  strfuint(order, buf);
  print(buf, PRINT_FLAG_BOTH);
  print("\n", PRINT_FLAG_BOTH);
#endif
  return index ^ (1ULL << order);
}

// Check if a block is allocated in the bitmap
static inline int buddy_is_allocated(uint64_t phys_addr) {
  uint64_t page_index = (phys_addr - buddy_base_addr) / BUDDY_PAGE_SIZE;
  uint64_t word_index = page_index / 64;
  uint64_t bit_index = page_index % 64;

#ifdef BUDDY_ALLOCATOR_DEBUG
  print("buddy_is_allocated: phys_addr = 0x", PRINT_FLAG_BOTH);
  char buf[20];
  hexstrfuint(phys_addr, buf);
  print(buf, PRINT_FLAG_BOTH);
  print(", word_index = ", PRINT_FLAG_BOTH);
  strfuint(word_index, buf);
  print(buf, PRINT_FLAG_BOTH);
  print(", bit_index = ", PRINT_FLAG_BOTH);
  strfuint(bit_index, buf);
  print(buf, PRINT_FLAG_BOTH);
  print("\n", PRINT_FLAG_BOTH);
#endif

  if (word_index >= bitmap_size_words) {
#ifdef BUDDY_ALLOCATOR_DEBUG
    print("buddy_is_allocated: word_index out of bounds\n", PRINT_FLAG_BOTH);
#endif
    return 0;
  }

#ifdef BUDDY_ALLOCATOR_DEBUG
  print("buddy_is_allocated: checking bitmap word ", PRINT_FLAG_BOTH);
  strfuint(word_index, buf);
  print(buf, PRINT_FLAG_BOTH);
  print(", bit ", PRINT_FLAG_BOTH);
  strfuint(bit_index, buf);
  print(buf, PRINT_FLAG_BOTH);
  print("\n", PRINT_FLAG_BOTH);

  print("buddy_is_allocated: bitmap value = ", PRINT_FLAG_BOTH);
  uint64_t value = allocation_bitmap[word_index];
  hexstrfuint(value, buf);
  print(buf, PRINT_FLAG_BOTH);
  print("\n", PRINT_FLAG_BOTH);
#endif

  return (allocation_bitmap[word_index] >> bit_index) & 1;
}

// Mark block as allocated in bitmap
static inline void buddy_mark_allocated(uint64_t phys_addr, int order) {
  uint64_t block_size = BUDDY_BLOCK_SIZE(order);
  uint64_t pages_in_block = block_size / BUDDY_PAGE_SIZE;
  uint64_t start_page = (phys_addr - buddy_base_addr) / BUDDY_PAGE_SIZE;

#ifdef BUDDY_ALLOCATOR_DEBUG
  print("buddy_mark_allocated: phys_addr = 0x", PRINT_FLAG_BOTH);
  char buf[20];
  hexstrfuint(phys_addr, buf);
  print(buf, PRINT_FLAG_BOTH);
  print(", order = ", PRINT_FLAG_BOTH);
  strfuint(order, buf);
  print(buf, PRINT_FLAG_BOTH);
  print(", pages_in_block = ", PRINT_FLAG_BOTH);
  strfuint(pages_in_block, buf);
  print(buf, PRINT_FLAG_BOTH);
  print(", start_page = ", PRINT_FLAG_BOTH);
  strfuint(start_page, buf);
  print(buf, PRINT_FLAG_BOTH);
  print("\n", PRINT_FLAG_BOTH);
#endif

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

#ifdef BUDDY_ALLOCATOR_DEBUG
  print("buddy_mark_free: phys_addr = 0x", PRINT_FLAG_BOTH);
  char buf[20];
  hexstrfuint(phys_addr, buf);
  print(buf, PRINT_FLAG_BOTH);
  print(", order = ", PRINT_FLAG_BOTH);
  strfuint(order, buf);
  print(buf, PRINT_FLAG_BOTH);
  print(", pages_in_block = ", PRINT_FLAG_BOTH);
  strfuint(pages_in_block, buf);
  print(buf, PRINT_FLAG_BOTH);
  print(", start_page = ", PRINT_FLAG_BOTH);
  strfuint(start_page, buf);
  print(buf, PRINT_FLAG_BOTH);
  print("\n", PRINT_FLAG_BOTH);
#endif

  for (uint64_t i = 0; i < pages_in_block; i++) {
    uint64_t page_index = start_page + i;
    uint64_t word_index = page_index / 64;
    uint64_t bit_index = page_index % 64;

    if (word_index >= bitmap_size_words)
      continue;

    /* double-free detection */
    if ((allocation_bitmap[word_index] & (1ULL << bit_index)) == 0) {
      char page_str[20];
      hexstrfuint(phys_addr, page_str);
      panic_msg_no_cr("buddy_allocator: double free of page 0x");
      print(page_str, PRINT_FLAG_BOTH);
      print("\n", PRINT_FLAG_BOTH);
      panic_halt();
    }

    allocation_bitmap[word_index] &= ~(1ULL << bit_index);
  }
}

// Check if entire buddy block is free
static inline int buddy_is_buddy_free(uint64_t buddy_addr, int order) {
  uint64_t block_size = BUDDY_BLOCK_SIZE(order);
  uint64_t pages_in_block = block_size / BUDDY_PAGE_SIZE;
  uint64_t start_page = (buddy_addr - buddy_base_addr) / BUDDY_PAGE_SIZE;

#ifdef BUDDY_ALLOCATOR_DEBUG
  print("buddy_is_buddy_free: buddy_addr = 0x", PRINT_FLAG_BOTH);
  char buf[20];
  hexstrfuint(buddy_addr, buf);
  print(buf, PRINT_FLAG_BOTH);
  print(", order = ", PRINT_FLAG_BOTH);
  strfuint(order, buf);
  print(buf, PRINT_FLAG_BOTH);
  print(", pages_in_block = ", PRINT_FLAG_BOTH);
  strfuint(pages_in_block, buf);
  print(buf, PRINT_FLAG_BOTH);
  print(", start_page = ", PRINT_FLAG_BOTH);
  strfuint(start_page, buf);
  print(buf, PRINT_FLAG_BOTH);
  print("\n", PRINT_FLAG_BOTH);
#endif

  for (uint64_t i = 0; i < pages_in_block; i++) {
    uint64_t page_index = start_page + i;
    uint64_t word_index = page_index / 64;
    uint64_t bit_index = page_index % 64;

    if (word_index >= bitmap_size_words) {
#ifdef BUDDY_ALLOCATOR_DEBUG
      print("buddy_is_buddy_free: word_index out of bounds\n", PRINT_FLAG_BOTH);
#endif
      return 0;
    }
    if ((allocation_bitmap[word_index] >> bit_index) & 1) {
#ifdef BUDDY_ALLOCATOR_DEBUG
      print("buddy_is_buddy_free: block not free at page index ",
            PRINT_FLAG_BOTH);
      strfuint(page_index, buf);
      print(buf, PRINT_FLAG_BOTH);
      print("\n", PRINT_FLAG_BOTH);
#endif
      return 0;
    }
  }
#ifdef BUDDY_ALLOCATOR_DEBUG
  print("buddy_is_buddy_free: block is free\n", PRINT_FLAG_BOTH);
#endif
  return 1;
}

// Remove block from free list
static void buddy_remove_from_free_list(struct buddy_block *block, int order) {
#ifdef BUDDY_ALLOCATOR_DEBUG
  print("buddy_remove_from_free_list: removing block at ", PRINT_FLAG_BOTH);
  char buf[20];
  hexstrfuint((uint64_t)block, buf);
  print(buf, PRINT_FLAG_BOTH);
  print(" from order ", PRINT_FLAG_BOTH);
  strfuint(order, buf);
  print(buf, PRINT_FLAG_BOTH);
  print("\n", PRINT_FLAG_BOTH);
#endif

  if (block->prev) {
#ifdef BUDDY_ALLOCATOR_DEBUG
    print("buddy_remove_from_free_list: removing from middle of free list\n",
          PRINT_FLAG_BOTH);
#endif
    block->prev->next = block->next;
  } else {
#ifdef BUDDY_ALLOCATOR_DEBUG
    print("buddy_remove_from_free_list: removing from head of free list\n",
          PRINT_FLAG_BOTH);
#endif
    free_lists[order] = block->next;
  }

  if (block->next) {
#ifdef BUDDY_ALLOCATOR_DEBUG
    print("buddy_remove_from_free_list: updating next block's prev\n",
          PRINT_FLAG_BOTH);
#endif
    block->next->prev = block->prev;
  }

#ifdef BUDDY_ALLOCATOR_DEBUG
  print("buddy_remove_from_free_list: block removed\n", PRINT_FLAG_BOTH);
#endif
  block->next = block->prev = NULL;
}

// Add block to front of free list
static void buddy_add_to_free_list(struct buddy_block *block, int order) {
#ifdef BUDDY_ALLOCATOR_DEBUG
  print("buddy_add_to_free_list: adding block at ", PRINT_FLAG_BOTH);
  char buf[20];
  hexstrfuint((uint64_t)block, buf);
  print(buf, PRINT_FLAG_BOTH);
  print(" to order ", PRINT_FLAG_BOTH);
  strfuint(order, buf);
  print(buf, PRINT_FLAG_BOTH);
  print("\n", PRINT_FLAG_BOTH);
#endif
  block->prev = NULL;
  block->next = free_lists[order];

  if (free_lists[order]) {
#ifdef BUDDY_ALLOCATOR_DEBUG
    print("buddy_add_to_free_list: adding to non-empty free list\n",
          PRINT_FLAG_BOTH);
#endif
    free_lists[order]->prev = block;
  }

  free_lists[order] = block;
}

// Find block in free list (for removal during coalescing)
static struct buddy_block *buddy_find_in_free_list(uint64_t phys_addr,
                                                   int order) {
  void *virt_addr = BUDDY_PHYS_TO_VIRT(phys_addr);
  struct buddy_block *current = free_lists[order];

#ifdef BUDDY_ALLOCATOR_DEBUG
  print("buddy_find_in_free_list: searching for block at ", PRINT_FLAG_BOTH);
  char buf[20];
  hexstrfuint(phys_addr, buf);
  print(buf, PRINT_FLAG_BOTH);
  print(" in order ", PRINT_FLAG_BOTH);
  strfuint(order, buf);
  print(buf, PRINT_FLAG_BOTH);
  print("\n", PRINT_FLAG_BOTH);
#endif

  while (current) {
    if (current == (struct buddy_block *)virt_addr) {
#ifdef BUDDY_ALLOCATOR_DEBUG
      print("buddy_find_in_free_list: block found at ", PRINT_FLAG_BOTH);
      hexstrfuint((uint64_t)current, buf);
      print(buf, PRINT_FLAG_BOTH);
      print("\n", PRINT_FLAG_BOTH);
#endif
      return current;
    }
    current = current->next;
  }

#ifdef BUDDY_ALLOCATOR_DEBUG
  print("buddy_find_in_free_list: block not found\n", PRINT_FLAG_BOTH);
#endif
  return NULL;
}

static inline int buddy_block_really_free(uint64_t buddy_addr, int order) {
  if (!buddy_is_buddy_free(buddy_addr, order))
    return 0;
  /* also make sure the block is actually in the free list */
  return buddy_find_in_free_list(buddy_addr, order) != NULL;
}

void buddy_allocator_init(struct limine_memmap_entry **entries,
                          uint64_t entry_count) {
#ifdef BUDDY_ALLOCATOR_DEBUG
  print("buddy_allocator_init: initializing buddy allocator\n",
        PRINT_FLAG_BOTH);
#endif

  if (is_buddy_allocator_initialized) {
    panic("buddy_allocator: already initialized");
  }

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

  // 0x80000000000fad18
  uint64_t satp_addr = (PS_get_atp() & 0xFFFFFFFFFFFFFF) << 12;

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

  is_buddy_allocator_initialized = true;
}

void *buddy_alloc_pages(int order) {
  if (order < 0 || order > BUDDY_MAX_ORDER) {
    return NULL;
  }

#ifdef BUDDY_ALLOCATOR_DEBUG
  print("buddy_alloc_pages: allocating order ", PRINT_FLAG_BOTH);
  char buf[20];
  strfuint(order, buf);
  print(buf, PRINT_FLAG_BOTH);
  print("\n", PRINT_FLAG_BOTH);
#endif

  // Look for free block of requested order or larger
  int current_order = order;
  while (current_order <= BUDDY_MAX_ORDER && !free_lists[current_order]) {
    current_order++;
  }

  if (current_order > BUDDY_MAX_ORDER) {
#ifdef BUDDY_ALLOCATOR_DEBUG
    print("buddy_alloc_pages: no free blocks available\n", PRINT_FLAG_BOTH);
#endif
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

#ifdef BUDDY_ALLOCATOR_DEBUG
  print("buddy_alloc_pages: allocated block at ", PRINT_FLAG_BOTH);
  hexstrfuint(block_addr, buf);
  print(buf, PRINT_FLAG_BOTH);
  print(", order = ", PRINT_FLAG_BOTH);
  strfuint(order, buf);
  print(buf, PRINT_FLAG_BOTH);
  print("\n", PRINT_FLAG_BOTH);
#endif

  return block;
}

void buddy_free_pages(void *ptr, int order) {
  if (!ptr || order < 0 || order > BUDDY_MAX_ORDER) {
    return;
  }

#ifdef BUDDY_ALLOCATOR_DEBUG
  print("buddy_free_pages: freeing block at ", PRINT_FLAG_BOTH);
  char buf[20];
  hexstrfuint((uint64_t)ptr, buf);
  print(buf, PRINT_FLAG_BOTH);
  print(", order = ", PRINT_FLAG_BOTH);
  strfuint(order, buf);
  print(buf, PRINT_FLAG_BOTH);
  print("\n", PRINT_FLAG_BOTH);
#endif

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
    if (!buddy_block_really_free(buddy_addr, current_order)) {
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
