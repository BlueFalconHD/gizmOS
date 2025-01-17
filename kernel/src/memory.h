#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>
#include <stddef.h>

/**
 * Copies n bytes from memory area src to memory area dest.
 *
 * @param dest Pointer to the destination memory area.
 * @param src Pointer to the source memory area.
 * @param n Number of bytes to copy.
 * @return Pointer to the destination memory area.
 */
void *memcpy(void *dest, const void *src, size_t n);

/**
 * Sets the first n bytes of the block of memory pointed by ptr to the specified value.
 *
 * @param dest Pointer to the block of memory to fill.
 * @param value Value to be set.
 * @param n Number of bytes to be set to the value.
 */
void *memset(void *s, int t, size_t n);

/**
 * Copies n bytes from memory area src to memory area dest. The memory areas may overlap.
 *
 * @param dest Pointer to the destination memory area.
 * @param src Pointer to the source memory area.
 * @param n Number of bytes to copy.
 * @return Pointer to the destination memory area.
 */
void *memmove(void *dest, const void *src, size_t n);

/**
 * Compares the first n bytes of two memory blocks.
 *
 * @param ptr1 Pointer to the first memory block.
 * @param ptr2 Pointer to the second memory block.
 * @param n Number of bytes to compare.
 * @return 0 if the contents of both memory blocks are equal, non-zero otherwise.
 */

int memcmp(const void *s1, const void *s2, size_t n);


/**
 * Swaps the endianness of a 16-bit integer.
 *
 * @param value The value to swap.
 */
uint16_t swap_uint16(uint16_t val);

/**
 * Swaps the endianness of a 32-bit integer.
 *
 * @param value The value to swap.
 */
uint32_t swap_uint32(uint32_t val);

/**
 * Swaps the endianness of a 64-bit integer.
 *
 * @param value The value to swap.
 */
uint64_t swap_uint64(uint64_t val);



#endif /* MEMORY_H */
