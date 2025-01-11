#include "memory.h"
#include <stdint.h>

uintptr_t mem_free; /* Global variable for next free memory address */

void *memcpy(void *dest, const void *src, size_t n) {
    uint8_t *pdest = (uint8_t *)dest;
    const uint8_t *psrc = (const uint8_t *)src;

    for (size_t i = 0; i < n; i++) {
        pdest[i] = psrc[i];
    }

    return dest;
}

void *memset(void *s, int c, size_t n) {
    uint8_t *p = (uint8_t *)s;

    for (size_t i = 0; i < n; i++) {
        p[i] = (uint8_t)c;
    }

    return s;
}

void *memmove(void *dest, const void *src, size_t n) {
    uint8_t *pdest = (uint8_t *)dest;
    const uint8_t *psrc = (const uint8_t *)src;

    if (src > dest) {
        for (size_t i = 0; i < n; i++) {
            pdest[i] = psrc[i];
        }
    } else if (src < dest) {
        for (size_t i = n; i > 0; i--) {
            pdest[i-1] = psrc[i-1];
        }
    }

    return dest;
}

int memcmp(const void *s1, const void *s2, size_t n) {
    const uint8_t *p1 = (const uint8_t *)s1;
    const uint8_t *p2 = (const uint8_t *)s2;

    for (size_t i = 0; i < n; i++) {
        if (p1[i] != p2[i]) {
            return p1[i] < p2[i] ? -1 : 1;
        }
    }

    return 0;
}

// Align to 8 bytes (can be adjusted as needed)
#define ALIGNMENT 8
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1))

// Block sizes
#define BLOCK_HEADER_SIZE (sizeof(Block))
#define BLOCK_FOOTER_SIZE (sizeof(size_t))
#define MIN_BLOCK_SIZE (BLOCK_HEADER_SIZE + BLOCK_FOOTER_SIZE + ALIGNMENT)

// Allocation flag manipulation (uses LSB of size)
#define SET_ALLOCATED(size) ((size) | 1)          // Set allocated flag
#define SET_FREE(size) ((size) & ~((size_t)1))    // Clear allocated flag
#define IS_ALLOCATED(size) ((size) & 1)           // Check allocated flag

// Block Structure Definitions

typedef struct Block {
    size_t size;                // Size of the block (including header and footer), LSB used as allocation flag
    struct Block* prev_free;    // Pointer to previous free block in the free list
    struct Block* next_free;    // Pointer to next free block in the free list
} Block;

// Global Variables

static uint8_t* heap_start = NULL;   // Start of the heap memory
static size_t heap_size = 0;         // Total size of the heap memory
static Block* free_list_head = NULL; // Head of the free list

// Function Implementations

void memory_init(void* heap, size_t size) {
    // Adjust heap start to be aligned
    uintptr_t aligned_heap_start = ALIGN((uintptr_t)heap);
    heap_size = size - (aligned_heap_start - (uintptr_t)heap);
    heap_size &= ~(ALIGNMENT - 1); // Align the heap size

    heap_start = (uint8_t*)aligned_heap_start;

    // Initialize the initial free block
    Block* initial_block = (Block*)heap_start;
    initial_block->size = SET_FREE(heap_size);
    initial_block->prev_free = NULL;
    initial_block->next_free = NULL;

    // Set the footer for the initial free block
    size_t* footer = (size_t*)(heap_start + heap_size - BLOCK_FOOTER_SIZE);
    *footer = initial_block->size;

    // Set the free list head
    free_list_head = initial_block;
}

void* malloc(size_t size) {
    if (size == 0) {
        return NULL;
    }

    // Align the requested size
    size = ALIGN(size);

    // Calculate total size needed (including header and footer)
    size_t total_size = size + BLOCK_HEADER_SIZE + BLOCK_FOOTER_SIZE;
    if (total_size < MIN_BLOCK_SIZE) {
        total_size = MIN_BLOCK_SIZE;
    }

    // Find a suitable free block using first-fit strategy
    Block* curr = free_list_head;
    while (curr) {
        size_t curr_size = SET_FREE(curr->size); // Size without allocation flag

        if (curr_size >= total_size) {
            // Block is large enough

            // Determine if we should split the block
            size_t remaining_size = curr_size - total_size;
            if (remaining_size >= MIN_BLOCK_SIZE) {
                // Split the block

                // New free block after allocated block
                Block* new_free_block = (Block*)((uint8_t*)curr + total_size);
                new_free_block->size = SET_FREE(remaining_size);
                new_free_block->prev_free = curr->prev_free;
                new_free_block->next_free = curr->next_free;

                // Update free list pointers
                if (new_free_block->next_free) {
                    new_free_block->next_free->prev_free = new_free_block;
                }
                if (new_free_block->prev_free) {
                    new_free_block->prev_free->next_free = new_free_block;
                } else {
                    free_list_head = new_free_block;
                }

                // Set footer for new free block
                size_t* new_free_footer = (size_t*)((uint8_t*)new_free_block + remaining_size - BLOCK_FOOTER_SIZE);
                *new_free_footer = new_free_block->size;

                // Prepare allocated block
                curr->size = SET_ALLOCATED(total_size);

                // Set footer for allocated block
                size_t* allocated_footer = (size_t*)((uint8_t*)curr + total_size - BLOCK_FOOTER_SIZE);
                *allocated_footer = curr->size;

            } else {
                // Allocate the entire block
                curr->size = SET_ALLOCATED(curr_size);

                // Set footer for allocated block
                size_t* allocated_footer = (size_t*)((uint8_t*)curr + curr_size - BLOCK_FOOTER_SIZE);
                *allocated_footer = curr->size;

                // Remove block from free list
                if (curr->prev_free) {
                    curr->prev_free->next_free = curr->next_free;
                } else {
                    free_list_head = curr->next_free;
                }
                if (curr->next_free) {
                    curr->next_free->prev_free = curr->prev_free;
                }
            }

            // Return pointer to user data
            return (void*)((uint8_t*)curr + BLOCK_HEADER_SIZE);
        }

        curr = curr->next_free;
    }

    // No suitable block found
    return NULL;
}

void free(void* ptr) {
    if (!ptr) {
        return;
    }

    // Get the block header
    Block* block = (Block*)((uint8_t*)ptr - BLOCK_HEADER_SIZE);

    // Mark the block as free
    size_t block_size = SET_FREE(block->size);
    block->size = block_size;

    // Update the footer
    size_t* footer = (size_t*)((uint8_t*)block + block_size - BLOCK_FOOTER_SIZE);
    *footer = block->size;

    Block* prev_block = NULL;
    Block* next_block = NULL;

    // Coalesce with previous block if possible
    if ((uint8_t*)block > heap_start) {
        // Check previous footer
        size_t* prev_footer = (size_t*)((uint8_t*)block - BLOCK_FOOTER_SIZE);
        size_t prev_size = *prev_footer;

        if (!IS_ALLOCATED(prev_size)) {
            // Previous block is free
            prev_block = (Block*)((uint8_t*)block - SET_FREE(prev_size));

            // Remove previous block from free list
            if (prev_block->prev_free) {
                prev_block->prev_free->next_free = prev_block->next_free;
            } else {
                free_list_head = prev_block->next_free;
            }
            if (prev_block->next_free) {
                prev_block->next_free->prev_free = prev_block->prev_free;
            }

            // Merge with previous block
            prev_block->size += block_size;
            // Update footer
            footer = (size_t*)((uint8_t*)prev_block + prev_block->size - BLOCK_FOOTER_SIZE);
            *footer = prev_block->size;

            block = prev_block;
            block_size = block->size;
        }
    }

    // Coalesce with next block if possible
    if ((uint8_t*)block + block_size < heap_start + heap_size) {
        next_block = (Block*)((uint8_t*)block + block_size);

        if (!IS_ALLOCATED(next_block->size)) {
            size_t next_size = SET_FREE(next_block->size);

            // Remove next block from free list
            if (next_block->prev_free) {
                next_block->prev_free->next_free = next_block->next_free;
            } else {
                free_list_head = next_block->next_free;
            }
            if (next_block->next_free) {
                next_block->next_free->prev_free = next_block->prev_free;
            }

            // Merge with next block
            block->size += next_size;
            // Update footer
            footer = (size_t*)((uint8_t*)block + block->size - BLOCK_FOOTER_SIZE);
            *footer = block->size;
        }
    }

    // Insert the coalesced block into the free list
    block->prev_free = NULL;
    block->next_free = free_list_head;
    if (free_list_head) {
        free_list_head->prev_free = block;
    }
    free_list_head = block;
}
