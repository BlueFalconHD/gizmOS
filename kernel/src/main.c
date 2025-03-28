#include "boot_time.h"
#include "device/framebuffer.h"
#include "device/rtc.h"
#include "device/term.h"
#include "dtb/dtb.h"
#include "hcf.h"
#include "hhdm.h"
#include "lib/fmt.h"
#include "lib/time.h"
#include "mem_layout.h"
#include "page_table.h"
#include "paging_mode.h"
#include "physical_alloc.h"
#include "tests/physical_alloc_test.h"
#include <device/uart.h>
#include <lib/mmio.h>
#include <lib/panic.h>
#include <lib/print.h>
#include <lib/str.h>
#include <limine.h>
#include <math.h>
#include <memory_map.h>
#include <stdbool.h>

#define VERSION "0.0.1"

// #define TESTS

__attribute__((
    used, section(".limine_requests"))) static volatile LIMINE_BASE_REVISION(3);

__attribute__((used,
               section(".limine_requests_"
                       "start"))) static volatile LIMINE_REQUESTS_START_MARKER;

__attribute__((
    used,
    section(
        ".limine_requests_end"))) static volatile LIMINE_REQUESTS_END_MARKER;

extern void trap_vector();

extern char kstart[]; // kernel start
                      // defined by linker script.

extern char kend[]; // first address after kernel.
                    // defined by linker script.

void main() {
  if (LIMINE_BASE_REVISION_SUPPORTED == false) {
    hcf();
  }

  char buffer[128];

  struct limine_framebuffer *fb = get_framebuffer();

  term_init(fb);

  paging_mode_init();
  hhdm_init();
  executable_address_init();
  executable_file_init();
  memory_map_init();
  dtb_init();
  boot_time_init();
  initialize_pages(memory_map_entries, memory_map_entry_count);

  printf("*. gizmOS %{type: str}\n", PRINT_FLAG_TERM, VERSION);

  // set trap vector
  asm volatile("csrw stvec, %0" ::"r"(&trap_vector));

#ifdef TESTS

  bool physical_alloc_test_results = run_physical_alloc_tests();
  if (!physical_alloc_test_results) {
    panic("Physical allocation tests failed");
  } else {
    print("Physical allocation tests passed\n");
    printf("Free pages: %{type: int}\n", get_free_page_count());
  }
#endif

  if ((uint64_t)kstart != executable_virtual_base) {
    panic_msg("Inconsistent kernel bases");
    hexstrfuint(executable_virtual_base, buffer);
    panic_msg_no_cr("Executable virtual base: ");
    term_puts(buffer);
    term_puts("\n");
    hexstrfuint((uint64_t)kstart, buffer);
    panic_msg_no_cr("Kernel start: ");
    term_puts(buffer);
    term_puts("\n");
  }

  page_table_t *root_page_table = create_page_table();
  if (!root_page_table) {
    panic("Failed to create root page table");
  }

  bool success = map_range(
      root_page_table, executable_virtual_base, executable_physical_base,
      (uint64_t)kend - (uint64_t)kstart, PTE_R | PTE_W | PTE_X | PTE_V);

  if (!success) {
    panic("Failed to set up kernel mapping");
  }

  success = map_range(root_page_table, hhdm_offset + 0xc0000000, 0xc0000000,
                      0xffffffff, PTE_R | PTE_W | PTE_X | PTE_V);

  if (!success) {
    panic("Failed to set up ram mapping");
  }

  mmio_map *mmap = alloc_mmio_map();

  mmio_map_add(mmap, 0x10000000, 0x1000, PTE_R | PTE_W | PTE_X | PTE_V,
               uart_mmio_callback);

  mmio_map_add(mmap, 0x101000, 0x1000, PTE_R | PTE_W | PTE_X | PTE_V,
               rtc_mmio_callback);

  mmio_map_pages(mmap, root_page_table);
  activate_page_table(root_page_table);
  mmio_map_was_activated(mmap);

  time_t initial_complete_time;
  unix_time_ns_to_time(goldfish_get_time(), &initial_complete_time);

  char buf2[128];
  time_to_string(&initial_complete_time, buf2);
  printf("\n%{type: str}\n\n", PRINT_FLAG_BOTH, buf2);

  panic("working as intended");
}
