#pragma once

#include <stddef.h>
#include <stdbool.h>

// uncomment to enable simple allocation tracing
// CURRENTLY BROKEN B/C OF RECURSION
// #define KALLOC_TRACE

typedef struct allocation {
  void *ptr;     /* Pointer to the allocated memory */
  size_t size;   /* Size of the allocated memory */
  char file[32]; /* File where the allocation was made */
  char line[8];  /* Line number in the file where the allocation was made */
} allocation_t;

extern allocation_t recent_allocations[100];

#ifdef KALLOC_TRACE
#define kalloc(size) kalloc_trace((size), __FILE__, __LINE__)
#define kfree(ptr) kfree_trace((ptr), __FILE__, __LINE__)
#define kresize(ptr, new_size)                                                 \
  kresize_trace((ptr), (new_size), __FILE__, __LINE__)
#else
#define kalloc(size) kalloc_impl((size))
#define kfree(ptr) kfree_impl((ptr))
#define kresize(ptr, new_size) kresize_impl((ptr), (new_size))
#endif

/**
 * Allocates a block of memory of the specified size.
 * This function is a wrapper around the actual kalloc_impl function, and it
 * takes in additional parameters about the allocation location.
 * @param size The size of the memory block to allocate.
 * @param file The file where the allocation is being made.
 * @param line The line number in the file where the allocation is being made.
 */
void *kalloc_trace(size_t size, const char *file, int line);

/**
 * Frees a block of memory that was previously allocated with kalloc.
 * This function is a wrapper around the actual kfree_impl function, and it
 * takes in additional parameters about the allocation location.
 * @param ptr The pointer to the memory block to free.
 * @param file The file where the allocation was made.
 * @param line The line number in the file where the allocation was made.
 * No size argument is required for kfree; the allocator tracks it internally.
 */
void kfree_trace(void *ptr, const char *file, int line);
void *kresize_trace(void *ptr, size_t new_size, const char *file, int line);

/**
 * True implementation of kalloc, which only handles the actual allocation not
 * tracing
 *
 * @param size The size of the memory block to allocate.
 * @return A pointer to the allocated memory block, or NULL if the allocation
 * failed
 */
void *kalloc_impl(size_t size);

/**
 * True implementation of kfree, which only handles the actual freeing of
 * memory.
 *
 * @param ptr The pointer to the memory block to free.
 */
void kfree_impl(void *ptr);

/**
 * True implementation of kresize, which only handles the actual resizing of
 * memory.
 *
 * @param ptr The pointer to the memory block to resize.
 * @param new_size The new size of the memory block.
 * @return A pointer to the resized memory block, or NULL if the resizing
 * failed
 */
void *kresize_impl(void *ptr, size_t new_size);

/**
 * Returns the total usable size in bytes of the memory block pointed to by ptr.
 * This may be greater than the originally requested size due to allocator
 * rounding. Returns 0 if ptr is NULL or not a kalloc()-managed pointer.
 */
size_t kalloc_usable_size(void *ptr);

/**
 * Returns true if ptr appears to be a kalloc-managed pointer
 * based on the expected kalloc header magic.
 */
bool kalloc_is_managed_pointer(void *ptr);
