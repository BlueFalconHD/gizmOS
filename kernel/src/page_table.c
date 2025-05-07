#include "page_table.h"
#include "lib/macros.h"
#include "lib/types.h"
#include <lib/memory.h> // For memset
#include <lib/panic.h>
#include <lib/print.h>
#include <lib/str.h>
#include <limine_requests.h>
#include <physical_alloc.h> // For alloc_page and free_page
#include <stdbool.h>

page_table_t *shared_page_table;

/**
 * @brief Helper function to convert a physical address to a virtual address.
 * @param pa Physical address.
 * @return Corresponding virtual address.
 */
static inline uint64_t pa_to_va(uint64_t pa) { return pa + hhdm_offset; }

/**
 * @brief Helper function to convert a virtual address to a physical address.
 * @param va Virtual address.
 * @return Corresponding physical address.
 */
static inline uint64_t va_to_pa(uint64_t va) { return va - hhdm_offset; }

/**
 * @brief Extract the VPN (Virtual Page Number) indices from a virtual address
 * for SV39.
 * @param va The virtual address.
 * @param vpn Output array of indices [level2, level1, level0].
 */
static void get_vpn_indices(uint64_t va, uint16_t vpn[SV39_LEVELS]) {
  vpn[0] = (va >> 12) & 0x1FF; /**< Level 0 index */
  vpn[1] = (va >> 21) & 0x1FF; /**< Level 1 index */
  vpn[2] = (va >> 30) & 0x1FF; /**< Level 2 index */
}

page_table_t *create_page_table() {
  page_table_t *pt = (page_table_t *)alloc_page();
  if (pt) {
    memset(pt, 0, PAGE_SIZE);
  }
  return pt;
}

bool map_page(page_table_t *root_table, uint64_t virtual_address,
              uint64_t physical_address, uint64_t flags) {
  if (!root_table) {
    panic_msg("Root page table is NULL");
    return false;
  }

  uint16_t vpn[SV39_LEVELS];
  get_vpn_indices(virtual_address, vpn);

  page_table_t *current_table = root_table;

  for (int level = SV39_LEVELS - 1; level >= 0; level--) {
    uint16_t index = vpn[level];
    pte_t entry = current_table->entries[index];

    if (level == 0) {
      pte_t pte = (physical_address >> 12) << 10; // PPN[2:0] in bits 53:10
      pte |= (flags & 0x3FF);                     // Flags are bits 9:0
      current_table->entries[index] = pte;
      return true;
    }

    if (entry & PTE_V) {
      uint64_t next_table_pa = (entry >> 10) << 12;
      uint64_t next_table_va = pa_to_va(next_table_pa);
      current_table = (page_table_t *)next_table_va;
    } else {
      page_table_t *new_table = create_page_table();
      if (!new_table) {
        panic_msg("Failed to allocate new page table");
        return false;
      }
      uint64_t new_table_pa = va_to_pa((uint64_t)new_table);
      pte_t pte = (new_table_pa >> 12) << 10; // PPN[2:0] in bits 53:10
      pte |= PTE_V;
      current_table->entries[index] = pte;
      current_table = new_table;
    }
  }

  panic_msg("Should not reach here; mapping failed");
  return false;
}

bool unmap_page(page_table_t *root_table, uint64_t virtual_address) {
  if (!root_table) {
    return false;
  }

  uint16_t vpn[SV39_LEVELS];
  get_vpn_indices(virtual_address, vpn);

  page_table_t *current_table = root_table;

  for (int level = SV39_LEVELS - 1; level >= 0; level--) {
    uint16_t index = vpn[level];
    pte_t entry = current_table->entries[index];

    if (!(entry & PTE_V)) {
      return false;
    }

    if (level == 0) {
      current_table->entries[index] = 0;
      return true;
    }

    uint64_t next_table_pa = (entry >> 10) << 12;
    uint64_t next_table_va = pa_to_va(next_table_pa);
    current_table = (page_table_t *)next_table_va;
  }

  return false;
}

bool get_physical_address(page_table_t *root_table, uint64_t virtual_address,
                          uint64_t *physical_address) {
  if (!root_table || !physical_address) {
    return false;
  }

  uint16_t vpn[SV39_LEVELS];
  get_vpn_indices(virtual_address, vpn);

  page_table_t *current_table = root_table;

  for (int level = SV39_LEVELS - 1; level >= 0; level--) {
    uint16_t index = vpn[level];
    pte_t entry = current_table->entries[index];

    if (!(entry & PTE_V)) {
      return false; // Entry not valid
    }

    if ((entry & (PTE_R | PTE_W | PTE_X)) != 0) {
      uint64_t ppn = entry >> 10;
      uint64_t offset = virtual_address & 0xFFF;
      *physical_address = (ppn << 12) | offset;
      return true;
    }

    uint64_t next_table_pa = (entry >> 10) << 12;
    uint64_t next_table_va = pa_to_va(next_table_pa);
    current_table = (page_table_t *)next_table_va;
  }

  return false;
}

bool identity_map(page_table_t *root_table, uint64_t start_address,
                  uint64_t size, uint64_t flags) {
  uint64_t addr = start_address;
  uint64_t end_addr = start_address + size;

  while (addr < end_addr) {
    if (!map_page(root_table, addr, addr, flags)) {

      panic_msg_no_cr("Failed to map page at ");
      char buffer[128];
      hexstrfuint(addr, buffer);
      print(buffer, PRINT_FLAG_BOTH);
      print("\n", PRINT_FLAG_BOTH);

      return false;
    }
    addr += PAGE_SIZE;
  }
  return true;
}

bool map_range(page_table_t *root_table, uint64_t virtual_start,
               uint64_t physical_start, uint64_t size, uint64_t flags) {
  if (!root_table) {
    panic_msg("Root page table is NULL");
    return false;
  }

  uint64_t addr_offset = 0;
  while (addr_offset < size) {
    uint64_t va = virtual_start + addr_offset;
    uint64_t pa = physical_start + addr_offset;

    if (!map_page(root_table, va, pa, flags)) {
      panic_msg_no_cr("Failed to map page at virtual address ");
      char buffer[128];
      hexstrfuint(va, buffer);
      print(buffer, PRINT_FLAG_BOTH);
      print("\n", PRINT_FLAG_BOTH);
      return false;
    }
    addr_offset += PAGE_SIZE;
  }
  return true;
}

bool unmap_range(page_table_t *root_table, uint64_t virtual_start,
                 uint64_t size) {
  if (!root_table) {
    return false;
  }

  uint64_t addr_offset = 0;
  while (addr_offset < size) {
    uint64_t va = virtual_start + addr_offset;

    if (!unmap_page(root_table, va)) {
      panic_msg_no_cr("Failed to unmap page at virtual address ");
      char buffer[128];
      hexstrfuint(va, buffer);
      print(buffer, PRINT_FLAG_BOTH);
      print("\n", PRINT_FLAG_BOTH);
      return false;
    }
    addr_offset += PAGE_SIZE;
  }
  return true;
}

void activate_page_table(page_table_t *root_table) {
  uint64_t root_table_pa = va_to_pa((uint64_t)root_table);
  uint64_t root_table_ppn = root_table_pa >> 12;
  uint64_t satp_value = (uint64_t)8ULL << 60; // MODE field for SV39
  satp_value |= root_table_ppn;
  asm volatile("csrw satp, %0" ::"r"(satp_value));
  asm volatile("sfence.vma");
}

g_bool is_addr_mapped(page_table_t *root_table, uint64_t virtual_address) {
  if (!root_table) {
    return false;
  }

  uint16_t vpn[SV39_LEVELS];
  get_vpn_indices(virtual_address, vpn);

  page_table_t *current_table = root_table;

  for (int level = SV39_LEVELS - 1; level >= 0; level--) {
    uint16_t index = vpn[level];
    pte_t entry = current_table->entries[index];

    if (!(entry & PTE_V)) {
      return false; // Entry not valid
    }

    if (level == 0) {

      // We are at level == 0. 'entry' has PTE_V set (checked before this
      // block).
      if ((entry & (PTE_R | PTE_W | PTE_X)) != 0) {
        // It's a leaf PTE (4KB page), so the page is mapped.
        uint64_t ppn = entry >> 10;
        uint64_t page_offset =
            virtual_address & 0xFFF; // Mask for 4KiB page offset
        uint64_t physical_address = (ppn << 12) | page_offset;

        // Debug print: <virt> -> <phys>
        char va_buf[20]; // Sufficient for "0x" + 16 hex digits + null
        char pa_buf[20];
        hexstrfuint(virtual_address, va_buf);
        hexstrfuint(physical_address, pa_buf);

        print(va_buf, PRINT_FLAG_BOTH);
        print(" -> ", PRINT_FLAG_BOTH);
        print(pa_buf, PRINT_FLAG_BOTH);
        print("\n", PRINT_FLAG_BOTH);

        return true;
      } else {
        // V is set, but R,W,X are all zero. This is a pointer PTE according to
        // RISC-V spec. At level 0, a pointer PTE means the address is not
        // mapped to a final page.
        return false;
      }
    }

    uint64_t next_table_pa = (entry >> 10) << 12;
    uint64_t next_table_va = pa_to_va(next_table_pa);
    current_table = (page_table_t *)next_table_va;
  }

  return false;
}
