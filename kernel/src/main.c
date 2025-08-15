#include "buddy_allocator.h"
#include "kprocs/cursor_daemon.h"
#include "kprocs/wallpaper_daemon.h"
#include "lib/canary.h"
#include "lib/dyn_array.h"
#include "lib/kalloc.h"
#include "lib/macros.h"
#include "lib/sbi.h"
#include "lib/timer.h"
#include "mem_layout.h"
#include "platform/interrupts.h"
#include "proc.h"
#include <device/console.h>
#include <device/framebuffer.h>
#include <device/plic.h>
#include <device/rtc.h>
#include <device/shared.h>
#include <device/uart.h>
#include <device/virtio/virtio_keyboard.h>
#include <dtb/dtb.h>
#include <lib/ansi.h>
#include <lib/gizm_font.h>
#include <lib/mmio.h>
#include <lib/print.h>
#include <lib/result.h>
#include <lib/str.h>
#include <lib/time.h>
#include <limine.h>
#include <limine_requests.h>
#include <memory_map.h>
#include <page_table.h>
#include <physical_alloc.h>
#include <platform/registers.h>
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

extern char trampoline[];

void enable_interrupts() {
  PS_enable_interrupts();
  PS_enable_all_interrupt_types();
}

G_INLINE void init_trap_vector(void) {
  /* point stvec at trap_vector */
  uintptr_t base = ((uintptr_t)&trap_vector) & ~0x3UL;
  PS_set_trap_vector(base);

  /* and preload sscratch with &trap_stack_top so the vector can
     switch to it immediately. */
  asm volatile("csrw sscratch, %0" ::"r"(&trap_stack_top));
}

extern uint8_t proc_ecall7_start[];
extern uint8_t proc_ecall7_end[];
extern uint8_t proc_ecall8_start[];
extern uint8_t proc_ecall8_end[];

DYN_ARRAY_DECLARE(uint64_t);
typedef DYN_ARRAY_TYPE(uint64_t) arr_test;

void main() {

  char buffer[128];

  limine_requests_init();

  // dtb_init();

  sbi_set_timer(UINT64_MAX);
  init_trap_vector();
  // sbi_set_timer(UINT64_MAX); // Disable timer interrupts initially

// canary_dbg_val((uint64_t)initialize_pages);
#ifndef NEW_ALLOC
  initialize_pages(memory_map_entries, memory_map_entry_count);
#else
  buddy_allocator_init(memory_map_entries, memory_map_entry_count);
#endif

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

  // bool success = true;

  if (!success) {
    panic("Failed to set up kernel mapping");
  }

  success = map_range(root_page_table, hhdm_offset + 0xc0000000, 0xc0000000,
                      0x100000000, PTE_R | PTE_W | PTE_X | PTE_V);

  if (!success) {
    panic("Failed to set up ram mapping");
  }

  uint64_t phys_lo = RAM_START; /* 0x8000_0000                       */
  uint64_t phys_hi = 0;         /* will become last byte of RAM      */
  for (uint64_t i = 0; i < memory_map_entry_count; i++) {
    struct limine_memmap_entry *e = memory_map_entries[i];
    if (e->type != LIMINE_MEMMAP_RESERVED) {
      uint64_t end = e->base + e->length;
      if (end > phys_hi)
        phys_hi = end;
    }
  }
  uint64_t ram_bytes = phys_hi - phys_lo;

  success =
      map_range(root_page_table, hhdm_offset + phys_lo, /* virtual start */
                phys_lo,                                /* physical start */
                ram_bytes,                              /* length in bytes */
                PTE_R | PTE_W | PTE_X | PTE_V);

  if (!success) {
    panic("Failed to set up ram mapping full");
  }

  mmio_map *mmap = alloc_mmio_map();
  mmio_map_add(mmap, 0x10000000, 0x1000, PTE_R | PTE_W | PTE_X | PTE_V,
               1); // UART
  mmio_map_add(mmap, 0x101000, 0x1000, PTE_R | PTE_W | PTE_X | PTE_V,
               1); // RTC
  mmio_map_add(mmap, 0x0C000000, 0x00600000, PTE_R | PTE_W | PTE_X | PTE_V,
               1); // PLIC
  // mmio_map_add(mmap, 0x2000000, 0x8000, PTE_R | PTE_W | PTE_X | PTE_V,
  // 1); // CLINT
  mmio_map_add(mmap, 0x10001000, 0x1000, PTE_R | PTE_W | PTE_X | PTE_V,
               1); // Virtio keyboard
  mmio_map_add(mmap, 0x10002000, 0x1000, PTE_R | PTE_W | PTE_X | PTE_V,
               1); // Virtio mouse
  mmio_map_add(mmap, 0x10003000, 0x1000, PTE_R | PTE_W | PTE_X | PTE_V,
               1); // Virtio gpu

  mmio_map_pages(mmap, root_page_table);
  activate_page_table(root_page_table);

  shared_page_table = root_page_table;

  // secondary page table resolution

  success = map_page(root_page_table, TRAMPOLINE, V2P((uint64_t)trampoline),
                     PTE_R | PTE_W | PTE_X | PTE_V);

  if (!success) {
    panic("Failed to set up trampoline mapping");
  }

  activate_page_table(root_page_table);

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

  // uart interrupt
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

  print_memory_map();

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

  // enable virtio gpu interrup
  // plic_set_priority(plic, 3, 1);
  // plic_enable_interrupt(plic, 0, PLIC_CONTEXT_SUPERVISOR, 3);

  // result_t rgpu = make_virtio_gpu(0x10003000, 3);
  // if (!result_is_ok(rgpu)) {
  //   panic("Failed to create virtio gpu");
  // }
  // virtio_gpu_t *gpu = (virtio_gpu_t *)result_unwrap(rgpu);
  // if (!virtio_gpu_init(gpu)) {
  //   panic("Failed to initialize virtio gpu");
  // }
  // set_shared_virtio_gpu(gpu);

  // init_trap_vector();

  sbi_set_timer(UINT64_MAX);

  enable_interrupts();
  uart_enable_interrupts(uart);

  printf("*. gizmOS %{type: str}\n", PRINT_FLAG_UART, VERSION);

  buddy_print_stats();

  void *tp[10];
  for (int i = 0; i < 10; i++) {
    tp[i] = buddy_alloc_page();
    if (!tp[i]) {
      panic("Failed to allocate test page");
    }
    printf("Allocated page %{type: hex}\n", PRINT_FLAG_BOTH, (uint64_t)tp[i]);
  }

  buddy_print_stats();

  // free the test pages
  for (int i = 0; i < 10; i++) {
    buddy_free_page(tp[i]);
    printf("Freed page %{type: hex}\n", PRINT_FLAG_BOTH, (uint64_t)tp[i]);
  }

  // test kalloc
  // void *kt = kalloc(128);
  // if (!kt) {
  //   panic("Failed to allocate kalloc test page");
  // }
  // printf("Allocated kalloc test page %{type: hex}\n", PRINT_FLAG_BOTH,
  //        (uint64_t)kt);
  // kfree(kt);
  // printf("Freed kalloc test page %{type: hex}\n", PRINT_FLAG_BOTH,
  //        (uint64_t)kt);

  initialize_processes();

  printf("(uint64_t)trampoline = %{type: hex}\n", PRINT_FLAG_BOTH,
         (uint64_t)trampoline);
  printf("V2P((uint64_t)trampoline) = %{type: hex}\n", PRINT_FLAG_BOTH,
         V2P((uint64_t)trampoline));

  // first_process();

  uint64_t size_ecall7 =
      (uint64_t)proc_ecall7_end - (uint64_t)proc_ecall7_start;
  proc_from_code(proc_ecall7_start, size_ecall7, "e7");

  uint64_t size_ecall8 =
      (uint64_t)proc_ecall8_end - (uint64_t)proc_ecall8_start;
  proc_from_code(proc_ecall8_start, size_ecall8, "e8");

  printf("Creating kernel tasks...\n", PRINT_FLAG_BOTH);

  result_t rcursor_task = make_kernel_task(cursor_daemon, NULL, "cursord");
  if (result_is_ok(rcursor_task)) {
    printf("Created cursord task\n", PRINT_FLAG_BOTH);
  } else {
    printf("Failed to create cursord task\n", PRINT_FLAG_BOTH);
  }

  result_t rwallpaper_task =
      make_kernel_task(wallpaper_daemon, NULL, "wallpaperd");
  if (result_is_ok(rwallpaper_task)) {
    printf("Created wallpaperd task\n", PRINT_FLAG_BOTH);
  } else {
    printf("Failed to create wallpaperd task\n", PRINT_FLAG_BOTH);
  }

  printf("Started kernel daemons with priority scheduling\n", PRINT_FLAG_BOTH);

  sbi_set_timer(get_csrr_time() + 1000000);

  scheduler();

  panic("hi");
}
