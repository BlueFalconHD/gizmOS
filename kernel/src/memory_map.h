#ifndef MEMORY_MAP_H
#define MEMORY_MAP_H

#include "limine.h"

extern struct limine_memmap_entry **memory_map_entries;
extern uint64_t memory_map_entry_count;

void memory_map_init(void);
const char *get_memmap_type_name(uint32_t type);

#endif /* MEMORY_MAP_H */
