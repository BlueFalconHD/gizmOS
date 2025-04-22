#include "lib/macros.h"
#include <device/clint.h>
#include <device/console.h>
#include <device/framebuffer.h>
#include <device/plic.h>
#include <device/rtc.h>
#include <device/shared.h>
#include <device/uart.h>
#include <device/virtio/virtio_keyboard.h>
#include <dtb/dtb.h>
#include <lib/ansi.h>
#include <lib/mmio.h>
#include <lib/panic.h>
#include <lib/print.h>
#include <lib/result.h>
#include <lib/str.h>
#include <lib/time.h>
#include <limine.h>
#include <limine_requests.h>
#include <memory_map.h>
#include <page_table.h>
#include <physical_alloc.h>
#include <stdbool.h>
#include <tests/trap_test.h>

#define VERSION "0.0.1"

// #define TESTS

extern void trap_vector();
extern char trap_stack_top; /* provided by trap.s */

extern char kstart[]; // kernel start
                      // defined by linker script.

extern char kend[]; // first address after kernel.
// defined by linker script.

void enable_interrupts() {
  // Enable global interrupts
  uint64_t sstatus;
  asm volatile("csrr %0, sstatus" : "=r"(sstatus));
  sstatus |= (1 << 1); // Set SIE bit (Supervisor Interrupt Enable)
  asm volatile("csrw sstatus, %0" : : "r"(sstatus));

  // Enable specific interrupt types
  uint64_t sie;
  asm volatile("csrr %0, sie" : "=r"(sie));
  sie |= (1 << 9); // Enable external interrupts (SEIE)
  sie |= (1 << 1); // Enable software interrupts (SSIE)
  asm volatile("csrw sie, %0" : : "r"(sie));

  // print("Enabled interrupts\n", PRINT_FLAG_BOTH);
}

G_INLINE void init_trap_vector(void) {
  /* point stvec at trap_vector … */
  uintptr_t base = ((uintptr_t)&trap_vector) & ~0x3UL;
  asm volatile("csrw stvec, %0" ::"r"(base));

  /* …and preload sscratch with &trap_stack_top so the vector can
     switch to it immediately. */
  asm volatile("csrw sscratch, %0" ::"r"(&trap_stack_top));
}

void main() {

  char buffer[128];

  limine_requests_init();

  dtb_init();
  initialize_pages(memory_map_entries, memory_map_entry_count);

  struct limine_framebuffer *lfb =
      limine_req_framebuffer.response->framebuffers[0];
  result_t rfb = make_framebuffer(lfb);
  if (!result_is_ok(rfb)) {
    panic("Failed to create framebuffer");
  }
  framebuffer_t *fb = (framebuffer_t *)result_unwrap(rfb);
  set_shared_framebuffer(fb);
  if (!framebuffer_init(fb)) {
    panic("Failed to initialize framebuffer");
  }

  result_t rconsole = make_console(fb);
  console_t *console = (console_t *)result_unwrap(rconsole);
  if (!console_init(console)) {
    panic("Failed to initialize console");
  }
  set_shared_console(console);

  asm volatile("csrw stvec, %0" ::"r"(&trap_vector));

  printf("*. gizmOS %{type: str}\n", PRINT_FLAG_BOTH, VERSION);

#ifdef TESTS

  bool physical_alloc_test_results = run_physical_alloc_tests();
  if (!physical_alloc_test_results) {
    panic("Physical allocation tests failed");
  } else {
    print("Physical allocation tests passed\n", PRINT_FLAG_BOTH);
    printf("Free pages: %{type: int}\n", PRINT_FLAG_BOTH,
           get_free_page_count());
  }
#endif

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
                      0x100000000, PTE_R | PTE_W | PTE_X | PTE_V);

  if (!success) {
    panic("Failed to set up ram mapping");
  }

  mmio_map *mmap = alloc_mmio_map();
  mmio_map_add(mmap, 0x10000000, 0x1000, PTE_R | PTE_W | PTE_X | PTE_V,
               1); // UART
  mmio_map_add(mmap, 0x101000, 0x1000, PTE_R | PTE_W | PTE_X | PTE_V, 1); // RTC
  mmio_map_add(mmap, 0x0C000000, 0x00600000, PTE_R | PTE_W | PTE_X | PTE_V,
               1); // PLIC
  mmio_map_add(mmap, 0x10001000, 0x1000, PTE_R | PTE_W | PTE_X | PTE_V,
               1); // Virtio keyboard
  mmio_map_add(mmap, 0x10002000, 0x1000, PTE_R | PTE_W | PTE_X | PTE_V,
               1); // Virtio mouse

  mmio_map_pages(mmap, root_page_table);
  activate_page_table(root_page_table);

  shared_page_table = root_page_table;

  result_t ruart = make_uart(0x10000000);
  if (!result_is_ok(ruart)) {
    panic("Failed to create UART");
  }
  uart_t *uart = (uart_t *)result_unwrap(ruart);
  if (!uart_init(uart)) {
    panic("Failed to initialize UART");
  }
  set_shared_uart(uart);

  result_t rrtc = make_rtc(0x101000);
  if (!result_is_ok(rrtc)) {
    panic("Failed to create RTC");
  }
  rtc_t *rtc = (rtc_t *)result_unwrap(rrtc);
  if (!rtc_init(rtc)) {
    panic("Failed to initialize RTC");
  }
  set_shared_rtc(rtc);
  result_t rplic = make_plic(0x0C000000);
  plic_t *plic = (plic_t *)result_unwrap(rplic);
  if (!plic_init(plic)) {
    panic("Failed to initialize PLIC");
  }

  set_shared_plic(plic);

  result_t rcursor = make_cursor(fb);
  if (!result_is_ok(rcursor)) {
    panic("Failed to create cursor");
  }
  cursor_t *cursor = (cursor_t *)result_unwrap(rcursor);
  if (!cursor_init(cursor, (int32_t)(lfb->width / 2),
                   (int32_t)(lfb->height / 2))) {
    panic("Failed to initialise cursor");
  }
  set_shared_cursor(cursor);

  plic_set_priority(plic, 10, 1);
  plic_set_threshold(plic, 0, PLIC_CONTEXT_SUPERVISOR, 0);
  plic_enable_interrupt(plic, 0, PLIC_CONTEXT_SUPERVISOR, 10);

  // enable virtio keyboard interrupt
  plic_set_priority(plic, 1, 1);
  plic_enable_interrupt(plic, 0, PLIC_CONTEXT_SUPERVISOR, 1);

  result_t rkbd = make_virtio_keyboard(0x10001000, 1);
  if (!result_is_ok(rkbd)) {
    panic("Failed to create virtio keyboard");
  }
  virtio_keyboard_t *kbd = (virtio_keyboard_t *)result_unwrap(rkbd);
  if (!virtio_keyboard_init(kbd)) {
    panic("Failed to initialize virtio keyboard");
  }
  set_shared_virtio_keyboard(kbd);

  // enable virtio mouse interrupt
  plic_set_priority(plic, 2, 1);
  plic_enable_interrupt(plic, 0, PLIC_CONTEXT_SUPERVISOR, 2);

  result_t rmouse = make_virtio_mouse(0x10002000, 2);
  if (!result_is_ok(rmouse)) {
    panic("Failed to create virtio mouse");
  }
  virtio_mouse_t *mouse = (virtio_mouse_t *)result_unwrap(rmouse);
  if (!virtio_mouse_init(mouse)) {
    panic("Failed to initialize virtio mouse");
  }
  set_shared_virtio_mouse(mouse);

  init_trap_vector();

  print("HEREjkjl;kjl;jk", PRINT_FLAG_BOTH);

  enable_interrupts();
  uart_enable_interrupts(uart);

  printf("*. gizmOS %{type: str}\n", PRINT_FLAG_UART, VERSION);

  // Print unallocated memory
  printf(ANSI_APPLY(ANSI_EFFECT_BOLD, "Free: ") "%{type: int} pages\n",
         PRINT_FLAG_BOTH, get_free_page_count());

  panic_loc("end of main");
}
