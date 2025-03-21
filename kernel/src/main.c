#include "boot_time.h"
#include "device/framebuffer.h"
#include "device/term.h"
#include "dtb/dtb.h"
#include "hcf.h"
#include "hhdm.h"
#include "lib/fmt.h"
#include "paging_mode.h"
#include "physical_alloc.h"
#include "tests/physical_alloc_test.h"
#include <device/uart.h>
#include <lib/panic.h>
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

extern void gicInit(void);
extern uint32_t readIAR0(void);
extern void writeEOIR0(uint32_t);

// defined in timer.s
extern void setTimerPeriod(uint32_t);
extern void enableTimer(void);
extern void disableTimer(void);

volatile uint32_t flag;

void fiqHandler(void) {
  uint32_t intid;
  intid = readIAR0(); // Read the interrupt id
  term_puts(format("Interrupt %{type: int} occurred\n", intid));

  if (intid == 29) {
    disableTimer();
    flag = 1;
  } else
    term_puts("Another interrupt occurred??\n");

  writeEOIR0(intid); // Clear interrupt
  return;
}

void main() {
  if (LIMINE_BASE_REVISION_SUPPORTED == false) {
    hcf();
  }

  char buffer[128];

  struct limine_framebuffer *fb = get_framebuffer();

  term_init(fb);
  // boot_time_init();

  hhdm_init();
  memory_map_init();
  dtb_init();
  paging_mode_init();
  initialize_pages(memory_map_entries, memory_map_entry_count);

  // uartInit((void *)0x09000000 + hhdm_offset);
  //
  // uart_puts("Hello, world!\n");

#ifdef TESTS
  bool physical_alloc_test_results = run_physical_alloc_tests();
  if (!physical_alloc_test_results) {
    print_error("physical alloc tests failed\n");
    hcf();
  }
#endif

  term_puts("all components initialized\n");

  term_puts(format("Hello, %{type: str}\n", "world!"));

  // Get current EL
  uint64_t el;
  asm volatile("mrs %0, CurrentEL" : "=r"(el));
  // Shift to get the actual EL
  el >>= 2;
  term_puts(format("Current EL: %{type: uint}\n", el));

  uartInit((void *)0x09000000);

  uart_puts("Hello, world!\n");
}
