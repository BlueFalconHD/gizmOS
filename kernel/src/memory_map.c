#include "memory_map.h"
#include "device/term.h"
#include "hcf.h"
#include "lib/panic.h"
#include "limine.h"
#include <lib/str.h>

__attribute__((
    used,
    section(".limine_requests"))) static volatile struct limine_memmap_request
    memory_map_request = {.id = LIMINE_MEMMAP_REQUEST, .revision = 0};

struct limine_memmap_entry **memory_map_entries;
uint64_t memory_map_entry_count;

void memory_map_init() {
  char buffer[128];

  struct limine_memmap_response *memory_map_response =
      memory_map_request.response;
  if (memory_map_response == NULL) {
    panic("Response to memory map request to Limine was null\n");
  } else {
    strfuint(memory_map_response->entry_count, buffer);
    memory_map_entries = memory_map_response->entries;
    memory_map_entry_count = memory_map_response->entry_count;
  }
}

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
  char buffer[128];

  term_puts("Memory map:\n");
  for (uint64_t i = 0; i < memory_map_entry_count; i++) {
    struct limine_memmap_entry *entry = memory_map_entries[i];
    hexstrfuint(entry->base, buffer);
    term_puts("  ");
    term_puts(buffer);
    term_puts(" - ");
    hexstrfuint(entry->base + entry->length, buffer);
    term_puts(buffer);
    term_puts(" (");
    strfuint(entry->length, buffer);
    term_puts(buffer);
    term_puts(" bytes, ");
    term_puts(get_memmap_type_name(entry->type));
    term_puts(")\n");
  }
}
