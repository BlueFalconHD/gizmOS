#include "memory_map.h"
#include "limine_requests.h"
#include <lib/panic.h>
#include <lib/print.h>
#include <lib/str.h>
#include <limine.h>
#include <stdint.h>

const char *get_memmap_type_name(uint32_t type) {
  switch (type) {
  case 0:
    return "USABLE";
  case 1:
    return "RESERVED";
  case 2:
    return "ACPI_RECLAIMABLE";
  case 3:
    return "ACPI_NVS";
  case 4:
    return "BAD_MEMORY";
  case 5:
    return "BOOTLOADER_RECLAIMABLE";
  case 6:
    return "EXECUTABLE_AND_MODULES";
  case 7:
    return "FRAMEBUFFER";
  default:
    return "UNKNOWN?";
  }
}

void print_memory_map() {
  print("Memory map:\n", PRINT_FLAG_TERM);
  for (uint64_t i = 0; i < memory_map_entry_count; i++) {
    struct limine_memmap_entry *entry = memory_map_entries[i];
    printf("  0x%{type: hex} - 0x%{type: hex} (%{type: int} bytes, %s)\n",
           PRINT_FLAG_TERM, entry->base, entry->base + entry->length,
           entry->length, get_memmap_type_name(entry->type));
  }
}
