#pragma once

#include <lib/types.h>
#include <stddef.h>

typedef struct allocation {
  void *ptr;     /* Pointer to the allocated memory */
  size_t size;   /* Size of the allocated memory */
  char file[32]; /* File where the allocation was made */
  char line[8];  /* Line number in the file where the allocation was made */
} allocation_t;

extern allocation_t recent_allocations[100];

// macros for kalloc and kfree which also call the tracing functions
#ifdef KALLOC_TRACE
#define kalloc(size) kalloc_trace((size), __FILE__, __LINE__)
#define kfree(ptr) kfree_trace((ptr), __FILE__, __LINE__)
#else
#define kalloc(size) kalloc_impl((size))
#define kfree(ptr) kfree_impl((ptr))
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
 * Frees a block of memory that was previously allocated with kalloc_internal.
 * This function is a wrapper around the actual kfree_impl function, and it
 * takes in additional parameters about the allocation location.
 * @param ptr The pointer to the memory block to free.
 * @param file The file where the allocation was made.
 * @param line The line number in the file where the allocation was made.
 *
 * Currently the file and line parameters are not used, but they can be useful
 * for debugging purposes.
 */
void kfree_trace(void *ptr, const char *file, int line);

/**
 * True implementation of kalloc, which only handles the actual allocation not
 * tracing
 */
void *kalloc_impl(size_t size);

/**
 * True implementation of kfree, which only handles the actual freeing of
 * memory.
 */
void kfree_impl(void *ptr);
