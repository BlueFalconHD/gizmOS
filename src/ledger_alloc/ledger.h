#ifndef LEDGER_H
#define LEDGER_H

#include <stddef.h>
#include <stdint.h>

/**
 * Initializes the memory allocator with the provided memory region.
 *
 * @param heap Pointer to the beginning of the memory region.
 * @param size Size of the memory region in bytes.
 */
void ledger_init(void* heap, size_t size);

/**
 * Allocates a block of memory of the specified size.
 *
 * @param size Size of the memory block to allocate in bytes.
 * @return Pointer to the allocated memory block, or NULL if allocation fails.
 */
void* ledger_malloc(size_t size);

/**
 * Frees a previously allocated block of memory.
 *
 * @param ptr Pointer to the memory block to free.
 */
void ledger_free(void* ptr);

#endif // LEDGER_H
