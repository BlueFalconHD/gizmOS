#include "mmio.h"
#include "lib/debug.h"

#include <lib/kalloc.h>
#include <lib/panic.h>
#include <page_table.h>
#include <stdbool.h>

mmio_map *alloc_mmio_map() {
  mmio_map *mmap = (mmio_map *)kalloc(sizeof(mmio_map));
  if (!mmap) {
    dbg("mmap == NULL");
    panic("Failed to allocate memory for mmio map");
  }
  mmap->count = 0;
  mmap->capacity = 16; /* initial */
  mmap->entries = (mmio_entry *)kalloc(mmap->capacity * sizeof(mmio_entry));
  if (!mmap->entries) {
    dbg("mmap->entries == NULL");
    panic("Failed to allocate memory for mmio entries");
  }
  return mmap;
}

bool mmio_map_add(mmio_map *map, uint64_t base, uint64_t size, uint64_t flags,
                  uint16_t id) {
  // Check if the map is null
  if (!map) {
    dbg("map == NULL");
    return false;
  }

  // grow if full
  if (map->count >= map->capacity) {
    uint64_t new_cap = map->capacity ? map->capacity * 2 : 16;
    mmio_entry *new_entries =
        (mmio_entry *)kalloc(new_cap * sizeof(mmio_entry));
    if (!new_entries) {
      dbg("new_entries == NULL");
      return false;
    }
    // copy existing
    for (uint64_t i = 0; i < map->count; i++) {
      new_entries[i] = map->entries[i];
    }
    kfree(map->entries);
    map->entries = new_entries;
    map->capacity = new_cap;
  }

  for (uint64_t i = 0; i < map->count; i++) {
    if (map->entries[i].base == base) {
      return false;
    }
  }

  map->entries[map->count].base = base;
  map->entries[map->count].size = size;
  map->entries[map->count].flags = flags;
  map->entries[map->count].id = id;
  map->count++;
  return true;
}

bool mmio_map_remove(mmio_map *map, uint64_t base) {
  if (!map) {
    dbg("map == NULL");
    return false;
  }

  for (uint64_t i = 0; i < map->count; i++) {
    if (map->entries[i].base == base) {
      for (uint64_t j = i; j < map->count - 1; j++) {
        map->entries[j] = map->entries[j + 1];
      }
      map->count--;
      return true;
    }
  }

  dbg("entry not found to remove");
  return false;
}

bool mmio_map_pages(mmio_map *map, page_table_t *pt) {
  if (!map) {
    dbg("map == NULL");
    return false;
  }

  if (!pt) {
    dbg("pt == NULL");
    return false;
  }

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
  if (!map) {
    dbg("map == NULL");
    return false;
  }

  if (!pt) {
    dbg("pt == NULL");
    return false;
  }

  // Unmap the entries
  for (uint64_t i = 0; i < map->count; i++) {
    for (uint64_t j = 0; j < map->entries[i].size; j += PAGE_SIZE) {
      if (!unmap_page(pt, map->entries[i].base + j)) {
        dbg("unmap_page(...) == false");
        return false;
      }
    }
  }

  return true;
}

static inline void mmio_map_free(mmio_map *map) {
  if (!map) {
    dbg("map == NULL");
    return;
  }
  if (map->entries) {
    kfree(map->entries);
    map->entries = NULL;
  }
  kfree(map);
}

/**
 * Checks if a given virtual address is mapped in the MMIO map.
 * @param map The MMIO map to check.
 * @param vaddr The virtual address to check.
 * @return The ID of the mapped entry if found, otherwise 0.
 */
uint16_t mmio_is_mapped(mmio_map *map, uint64_t vaddr) {
  if (!map) {
    dbg("map == NULL");
    return 0;
  }

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
  if (!map) {
    dbg("map == NULL");
    return 0;
  }

  if (size != 1 && size != 2 && size != 4) {
    dbg("size != 1 && size != 2 && size != 4");
    return 0;
  }

  for (uint64_t i = 0; i < map->count; i++) {
    if (vaddr >= map->entries[i].base &&
        vaddr < map->entries[i].base + map->entries[i].size) {
      if (rid != 0 && rid != map->entries[i].id) {
        dbg("rid != 0 && rid != map->entries[i].id");
        return 0;
      }

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
  if (!map) {
    dbg("map == NULL");
    return;
  }

  if (size != 1 && size != 2 && size != 4) {
    dbg("size != 1 && size != 2 && size != 4");
    return;
  }

  for (uint64_t i = 0; i < map->count; i++) {
    if (vaddr >= map->entries[i].base &&
        vaddr < map->entries[i].base + map->entries[i].size) {
      if (rid != 0 && rid != map->entries[i].id) {
        dbg("rid != 0 && rid != map->entries[i].id");
        return;
      }

      volatile uint8_t *addr = (volatile uint8_t *)(vaddr);
      for (uint8_t j = 0; j < size; j++) {
        addr[j] = (value >> (j * 8)) & 0xFF;
      }
      return;
    }
  }
}
