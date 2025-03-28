#pragma once

#include <page_table.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct {
  uint64_t base;
  uint64_t size;
  uint64_t flags;

  // optional callback to call when a page is mapped
  void (*page_callback)(uint64_t addr);
} mmio_entry;

// how many entries (plus main mmio_map struct) can be stored in 4KiB
#define MAX_MMIO_ENTRIES ((4096 - sizeof(mmio_map)) / sizeof(mmio_entry))

typedef struct {
  mmio_entry *entries;
  uint64_t count;
} mmio_map;

mmio_map *alloc_mmio_map();

bool mmio_map_add(mmio_map *map, uint64_t base, uint64_t size, uint64_t flags,
                  void (*page_callback)(uint64_t addr));
bool mmio_map_remove(mmio_map *map, uint64_t base);
bool mmio_map_pages(mmio_map *map, page_table_t *pt);
bool mmio_map_contains(mmio_map *map, uint64_t addr);

void mmio_map_was_activated(mmio_map *map);
