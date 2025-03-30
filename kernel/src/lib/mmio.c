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

bool mmio_map_add(mmio_map *map, uint64_t base, uint64_t size, uint64_t flags) {
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
