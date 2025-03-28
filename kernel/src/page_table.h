#ifndef PAGE_TABLE_H
#define PAGE_TABLE_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @file page_table.h
 * @brief Page table management for RISC-V SV39 paging mode.
 */

/**
 * @defgroup PageTable Page Table Management
 * @{
 */

/**
 * @brief Number of page table levels in SV39 mode.
 */
#define SV39_LEVELS 3

/**
 * @brief Page size in bytes.
 */
#define PAGE_SIZE 4096

/**
 * @brief Page table entry flags.
 */
enum page_entry_flags {
  PTE_V = 1 << 0,    /**< Valid */
  PTE_R = 1 << 1,    /**< Read */
  PTE_W = 1 << 2,    /**< Write */
  PTE_X = 1 << 3,    /**< Execute */
  PTE_U = 1 << 4,    /**< User */
  PTE_G = 1 << 5,    /**< Global */
  PTE_A = 1 << 6,    /**< Accessed */
  PTE_D = 1 << 7,    /**< Dirty */
  PTE_RSW1 = 1 << 8, /**< Reserved for software use */
  PTE_RSW2 = 1 << 9, /**< Reserved for software use */
};

/**
 * @brief Type definition for a page table entry in SV39.
 */
typedef uint64_t pte_t;

/**
 * @brief Definition of a page table, consisting of 512 entries.
 */
typedef struct page_table {
  pte_t entries[512]; /**< Array of page table entries */
} page_table_t;

/**
 * @brief Initialize a new page table.
 * @return Pointer to the newly allocated and zero-initialized page table.
 */
page_table_t *create_page_table();

/**
 * @brief Map a virtual address to a physical address with specified
 * permissions.
 * @param root_table Pointer to the root page table.
 * @param virtual_address The virtual address to map.
 * @param physical_address The physical address to map to.
 * @param flags Permissions and flags for the mapping (combination of PTE_*
 * flags).
 * @return `true` on success, `false` on failure.
 */
bool map_page(page_table_t *root_table, uint64_t virtual_address,
              uint64_t physical_address, uint64_t flags);

/**
 * @brief Unmap a virtual address.
 * @param root_table Pointer to the root page table.
 * @param virtual_address The virtual address to unmap.
 * @return `true` on success, `false` on failure.
 */
bool unmap_page(page_table_t *root_table, uint64_t virtual_address);

/**
 * @brief Look up the physical address mapped to a virtual address.
 * @param root_table Pointer to the root page table.
 * @param virtual_address The virtual address to look up.
 * @param physical_address Output parameter for the physical address.
 * @return `true` if mapping exists, `false` otherwise.
 */
bool get_physical_address(page_table_t *root_table, uint64_t virtual_address,
                          uint64_t *physical_address);

/**
 * @brief Set up an identity mapping for a range of addresses.
 * This is useful for early boot stages where the kernel needs to access
 * physical memory directly.
 * @param root_table Pointer to the root page table.
 * @param start_address The start address of the range to map.
 * @param size The size of the range in bytes.
 * @param flags Permissions and flags for the mapping (combination of PTE_*
 * flags).
 * @return `true` on success, `false` on failure.
 */
bool identity_map(page_table_t *root_table, uint64_t start_address,
                  uint64_t size, uint64_t flags);

bool map_range(page_table_t *root_table, uint64_t virtual_start,
               uint64_t physical_start, uint64_t size, uint64_t flags);

/**
 * @brief Activate the given page table by writing its physical address to the
 * `satp` register.
 * @param root_table Pointer to the root page table to activate.
 */
void activate_page_table(page_table_t *root_table);

/** @} */

#endif // PAGE_TABLE_H
