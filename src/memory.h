#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>

#define MEM_START 0x40200000  /* Start 2 MB after the kernel start address */
#define MEM_SIZE  0x0FE00000  /* Adjust size to stay within RAM bounds */

extern uintptr_t mem_free;    /* Global variable for next free memory address */

void *memcpy(void *dest, const void *src, unsigned int n);

#endif /* MEMORY_H */
