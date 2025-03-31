#include "mmio.h"

#include <lib/panic.h>
#include <page_table.h>
#include <physical_alloc.h>
#include <stdbool.h>

mmio_map *alloc_mmio_map() {
  mmio_map *mmap = alloc_page();
  mmap->count = 0;
  mmap->entries = alloc_page();
  if (!mmap || !mmap->entries) {
    panic("Failed to allocate memory for mmio map");
  }

  return mmap;
}

bool mmio_map_add(mmio_map *map, uint64_t base, uint64_t size, uint64_t flags,
                  uint16_t id) {
  // Check if the map is null
  if (!map) {
    return false;
  }

  // Check if the map is full
  if (map->count >= MAX_MMIO_ENTRIES) {
    return false;
  }

  // Check if the base address is already in the map
  for (uint64_t i = 0; i < map->count; i++) {
    if (map->entries[i].base == base) {
      return false;
    }
  }

  // Add the entry
  map->entries[map->count].base = base;
  map->entries[map->count].size = size;
  map->entries[map->count].flags = flags;
  map->entries[map->count].id = id;
  map->count++;
  return true;
}

bool mmio_map_remove(mmio_map *map, uint64_t base) {
  // Check if the map is null
  if (!map) {
    return false;
  }

  // Find the entry
  for (uint64_t i = 0; i < map->count; i++) {
    if (map->entries[i].base == base) {
      // Remove the entry
      for (uint64_t j = i; j < map->count - 1; j++) {
        map->entries[j] = map->entries[j + 1];
      }
      map->count--;
      return true;
    }
  }

  return false;
}

bool mmio_map_pages(mmio_map *map, page_table_t *pt) {
  // Check if the map is null
  if (!map) {
    return false;
  }

  // Check if the page table is null
  if (!pt) {
    return false;
  }

  // Map the entries as identity map
  for (uint64_t i = 0; i < map->count; i++) {
    for (uint64_t j = 0; j < map->entries[i].size; j += PAGE_SIZE) {
      if (!map_page(pt, map->entries[i].base + j, map->entries[i].base + j,
                    map->entries[i].flags)) {
        return false;
      }
    }
  }

  return true;
}

bool mmio_unmap_pages(mmio_map *map, page_table_t *pt) {
  // Check if the map is null
  if (!map) {
    return false;
  }

  // Check if the page table is null
  if (!pt) {
    return false;
  }

  // Unmap the entries
  for (uint64_t i = 0; i < map->count; i++) {
    for (uint64_t j = 0; j < map->entries[i].size; j += PAGE_SIZE) {
      if (!unmap_page(pt, map->entries[i].base + j)) {
        return false;
      }
    }
  }

  return true;
}

/**
 * Checks if a given virtual address is mapped in the MMIO map.
 * @param map The MMIO map to check.
 * @param vaddr The virtual address to check.
 * @return The ID of the mapped entry if found, otherwise 0.
 */
uint16_t mmio_is_mapped(mmio_map *map, uint64_t vaddr) {
  // Check if the map is null
  if (!map) {
    return 0;
  }

  // Check if the address is contained in the map
  for (uint64_t i = 0; i < map->count; i++) {
    if (vaddr >= map->entries[i].base &&
        vaddr < map->entries[i].base + map->entries[i].size) {
      return map->entries[i].id;
    }
  }

  return 0;
}

/**
 * MMIO read function. Reads a value from a given MMIO address.
 * @param map The MMIO map to use.
 * @param vaddr The virtual address to read from.
 * @param size The size of the value to read (1, 2, or 4 bytes).
 * @param rid The enforced ID of the MMIO entry. Set to 0 to ignore.
 * @return The read value.
 */
uint64_t mmio_read(mmio_map *map, uint64_t vaddr, uint8_t size, uint16_t rid) {
  // Check if the map is null
  if (!map) {
    return 0;
  }

  // Check if the size is valid
  if (size != 1 && size != 2 && size != 4) {
    return 0;
  }

  // Check if the address is contained in the map
  for (uint64_t i = 0; i < map->count; i++) {
    if (vaddr >= map->entries[i].base &&
        vaddr < map->entries[i].base + map->entries[i].size) {
      // Check if the ID matches
      if (rid != 0 && rid != map->entries[i].id) {
        return 0;
      }

      // Read the value from the MMIO address
      volatile uint8_t *addr = (volatile uint8_t *)(vaddr);
      uint64_t value = 0;
      for (uint8_t j = 0; j < size; j++) {
        value |= ((uint64_t)addr[j]) << (j * 8);
      }
      return value;
    }
  }

  return 0;
}

/**
 * MMIO write function. Writes a value to a given MMIO address.
 * @param map The MMIO map to use.
 * @param vaddr The virtual address to write to.
 * @param value The value to write.
 * @param size The size of the value to write (1, 2, or 4 bytes).
 * @param rid The enforced ID of the MMIO entry. Set to 0 to ignore.
 */
void mmio_write(mmio_map *map, uint64_t vaddr, uint64_t value, uint8_t size,
                uint16_t rid) {
  // Check if the map is null
  if (!map) {
    return;
  }

  // Check if the size is valid
  if (size != 1 && size != 2 && size != 4) {
    return;
  }

  // Check if the address is contained in the map
  for (uint64_t i = 0; i < map->count; i++) {
    if (vaddr >= map->entries[i].base &&
        vaddr < map->entries[i].base + map->entries[i].size) {
      // Check if the ID matches
      if (rid != 0 && rid != map->entries[i].id) {
        return;
      }

      // Write the value to the MMIO address
      volatile uint8_t *addr = (volatile uint8_t *)(vaddr);
      for (uint8_t j = 0; j < size; j++) {
        addr[j] = (value >> (j * 8)) & 0xFF;
      }
      return;
    }
  }
}
