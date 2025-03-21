#include "page_table.h"
#include <lib/panic.h>
#include <physical_alloc.h>
#include <stdint.h>

void map_page(uint64_t *page_table_base, uint64_t va, uint64_t pa) {
  // Masks to extract indices
  const uint64_t L0_index = (va >> 39) & 0x1FF;
  const uint64_t L1_index = (va >> 30) & 0x1FF;
  const uint64_t L2_index = (va >> 21) & 0x1FF;
  const uint64_t L3_index = (va >> 12) & 0x1FF;

  uint64_t *l0_table = page_table_base;
  uint64_t *l1_table;
  uint64_t *l2_table;
  uint64_t *l3_table;

  // Check if L0 entry exists
  if (!(l0_table[L0_index] & PTE_VALID)) {
    // Allocate L1 table
    l1_table = allocate_page_table();
    l0_table[L0_index] =
        ((uint64_t)l1_table & (~0xFFF)) | PTE_VALID | PTE_TABLE;
  } else {
    l1_table = (uint64_t *)(l0_table[L0_index] & ~0xFFFUL);
  }

  if (!(l1_table[L1_index] & PTE_VALID)) {
    // Allocate L2 table
    l2_table = allocate_page_table();
    l1_table[L1_index] =
        ((uint64_t)l2_table & (~0xFFF)) | PTE_VALID | PTE_TABLE;
  } else {
    l2_table = (uint64_t *)(l1_table[L1_index] & ~0xFFFUL);
  }

  if (!(l2_table[L2_index] & PTE_VALID)) {
    // Allocate L3 table
    l3_table = allocate_page_table();
    l2_table[L2_index] =
        ((uint64_t)l3_table & (~0xFFF)) | PTE_VALID | PTE_TABLE;
  } else {
    l3_table = (uint64_t *)(l2_table[L2_index] & ~0xFFFUL);
  }

  // Now set the L3 entry
  l3_table[L3_index] = (pa & (~0xFFFUL)) | PTE_VALID | PTE_AF | PTE_AP_RW_EL1 |
                       PTE_MEMATTR_DEV_nGnRE;
}

uint64_t *allocate_page_table() {
  void *page = alloc_page();
  if (page == NULL) {
    panic("Out of memory while allocating page table");
  }
  // Zero out the page table
  memset(page, 0, PAGE_SIZE);
  return (uint64_t *)page;
}
