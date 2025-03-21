#ifndef PAGE_TABLE_H
#define PAGE_TABLE_H

#include <stdint.h>

#define PTE_VALID (1UL << 0)
#define PTE_TABLE (1UL << 1)
#define PTE_AF (1UL << 10)               // Access flag
#define PTE_SH_NS (0UL << 8)             // Non-shareable
#define PTE_SH_OS (2UL << 8)             // Outer-shareable
#define PTE_AP_RW_EL1 (0UL << 6)         // Read/Write EL1
#define PTE_MEMATTR_DEV_nGnRE (0UL << 2) // Device-nGnRE memory

void map_page(uint64_t *page_table_base, uint64_t va, uint64_t pa);
uint64_t *allocate_page_table();

#endif /* PAGE_TABLE_H */
