#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>
#include <stddef.h>


#define MEM_START 0x40200000  /* Start 2 MB after the kernel start address */
#define MEM_SIZE  0x0FE00000  /* Adjust size to stay within RAM bounds */

extern uintptr_t mem_free;    /* Global variable for next free memory address */

/**
 * Copies n bytes from memory area src to memory area dest.
 *
 * @param dest Pointer to the destination memory area.
 * @param src Pointer to the source memory area.
 * @param n Number of bytes to copy.
 * @return Pointer to the destination memory area.
 */
void *memcpy(void *dest, const void *src, unsigned int n);

/**
 * Initializes the memory allocator with the provided memory region.
 *
 * @param heap Pointer to the beginning of the memory region.
 * @param size Size of the memory region in bytes.
 */
void memory_init(void* heap, size_t size);

/**
 * Allocates a block of memory of the specified size.
 *
 * @param size Size of the memory block to allocate in bytes.
 * @return Pointer to the allocated memory block, or NULL if allocation fails.
 */
void* malloc(size_t size);

/**
 * Frees a previously allocated block of memory.
 *
 * @param ptr Pointer to the memory block to free.
 */
void free(void* ptr);

#endif /* MEMORY_H */
