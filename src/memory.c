#include "memory.h"

uintptr_t mem_free; /* Global variable for next free memory address */

void *memcpy(void *dest, const void *src, unsigned int n)
{
    unsigned char *d = dest;
    const unsigned char *s = src;
    while (n--) {
        *d++ = *s++;
    }
    return dest;
}
