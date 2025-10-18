#include "earlyinit.h"
#include "kprocs/tetris.h"
#include "lib/canary.h"
#include "lib/debug.h"
#include "lib/dyn_array.h"
#include "lib/macros.h"
#include "lib/sbi.h"
#include "lib/timer.h"
#include "mem_layout.h"
#include "platform/interrupts.h"
#include <device/console.h>
#include <device/disk.h>
#include <device/framebuffer.h>
#include <device/plic.h>
#include <device/rtc.h>
#include <device/shared.h>
#include <device/uart.h>
#include <device/virtio/drivers/block.h>
#include <device/virtio/virtio.h>
#include <dtb/dtb.h>
#include <fs/fat.h>
#include <kprocs/pixelcore_demo.h>
#include <lib/PixelCore/backbuffer.h>
#include <lib/PixelCore/rect.h>
#include <lib/PixelCore/region.h>
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
#include <platform/exception.h>
#include <platform/registers.h>
#include <proc/kernel_task.h>
#include <proc/lifecycle.h>
#include <proc/process.h>
#include <proc/process_table.h>
#include <proc/scheduler.h>
#include <stdbool.h>

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
extern uint8_t user_keynotify_start[];
extern uint8_t user_keynotify_end[];

EARLY_TEXT void main() {
  early_init_status eastat = early_init();
  (void)eastat;

  struct limine_framebuffer *lfb =
      limine_req_framebuffer.response->framebuffers[0];
  result_t rfb = make_framebuffer(lfb);
  if (!result_is_ok(rfb))
    panic("Failed to create framebuffer");
  framebuffer_t *fb = (framebuffer_t *)result_unwrap(rfb);
  set_shared_framebuffer(fb);
  if (!framebuffer_init(fb)) {
    dbg("framebuffer_init(...) == false");
    panic("Failed to initialize framebuffer");
  }

  result_t rconsole = make_console(fb);
  console_t *console = (console_t *)result_unwrap(rconsole);
  if (!console_init(console)) {
    dbg("console_init(...) == false");
    panic("Failed to initialize console");
  }
  set_shared_console(console);

  printf("*. gizmOS %{type: str}\n", PRINT_FLAG_BOTH, VERSION);

  mmio_map *mmap = alloc_mmio_map();
  mmio_map_add(mmap, 0x10000000, 0x1000, PTE_R | PTE_W | PTE_X | PTE_V,
               1); // UART
  mmio_map_add(mmap, 0x101000, 0x1000, PTE_R | PTE_W | PTE_X | PTE_V,
               1); // RTC
  mmio_map_add(mmap, 0x0C000000, 0x00600000, PTE_R | PTE_W | PTE_X | PTE_V,
               1); // PLIC
  mmio_map_add(mmap, 0x10001000, 0x1000, PTE_R | PTE_W | PTE_X | PTE_V,
               1); // Virtio keyboard
  mmio_map_add(mmap, 0x10002000, 0x1000, PTE_R | PTE_W | PTE_X | PTE_V,
               1); // Virtio mouse
  mmio_map_add(mmap, 0x10003000, 0x1000, PTE_R | PTE_W | PTE_X | PTE_V,
               1); // Virtio gpu
  mmio_map_add(mmap, 0x10004000, 0x1000, PTE_R | PTE_W | PTE_X | PTE_V,
               1); // Virtio block

  mmio_map_pages(mmap, shared_page_table);
  activate_page_table(shared_page_table);

  bool success =
      map_page(shared_page_table, TRAMPOLINE, V2P((uint64_t)trampoline),
               PTE_R | PTE_W | PTE_X | PTE_V);
  if (!success) {
    dbg("map_page(...) == false");
    panic("Failed to set up trampoline mapping");
  }

  activate_page_table(shared_page_table);

  result_t ruart = make_uart(0x10000000);
  if (!result_is_ok(ruart)) {
    dbg("make_uart(...) != OK");
    panic("Failed to create UART");
  }
  uart_t *uart = (uart_t *)result_unwrap(ruart);
  if (!uart_init(uart)) {
    dbg("uart_init(...) == false");
    panic("Failed to initialize UART");
  }
  set_shared_uart(uart);

  result_t rrtc = make_rtc(0x101000);
  if (!result_is_ok(rrtc)) {
    dbg("make_rtc(...) != OK");
    panic("Failed to create RTC");
  }
  rtc_t *rtc = (rtc_t *)result_unwrap(rrtc);
  if (!rtc_init(rtc)) {
    dbg("rtc_init(...) == false");
    panic("Failed to initialize RTC");
  }
  set_shared_rtc(rtc);

  result_t rplic = make_plic(0x0C000000);
  plic_t *plic = (plic_t *)result_unwrap(rplic);
  if (!plic_init(plic)) {
    dbg("plic_init(...) == false");
    panic("Failed to initialize PLIC");
  }
  set_shared_plic(plic);

  result_t rcursor = make_cursor(fb);
  if (!result_is_ok(rcursor)) {
    dbg("make_cursor(...) != OK");
    panic("Failed to create cursor");
  }
  cursor_t *cursor = (cursor_t *)result_unwrap(rcursor);
  if (!cursor_init(cursor, (int32_t)(lfb->width / 2),
                   (int32_t)(lfb->height / 2))) {
    dbg("cursor_init(...) == false");
    panic("Failed to initialise cursor");
  }
  set_shared_cursor(cursor);

  // uart interrupt
  plic_set_priority(plic, 10, 1);
  plic_set_threshold(plic, 0, PLIC_CONTEXT_SUPERVISOR, 0);
  plic_enable_interrupt(plic, 0, PLIC_CONTEXT_SUPERVISOR, 10);

  // virtio
  plic_set_priority(plic, 1, 1);
  plic_enable_interrupt(plic, 0, PLIC_CONTEXT_SUPERVISOR, 1);
  plic_set_priority(plic, 2, 1);
  plic_enable_interrupt(plic, 0, PLIC_CONTEXT_SUPERVISOR, 2);
  plic_set_priority(plic, 3, 1);
  plic_enable_interrupt(plic, 0, PLIC_CONTEXT_SUPERVISOR, 3);

  sbi_set_timer(UINT64_MAX);

  enable_interrupts();
  uart_enable_interrupts(uart);

  printf("*. gizmOS %{type: str}\n", PRINT_FLAG_UART, VERSION);

  initialize_processes();

  // Launch user process that registers for keypress notifications and prints
  // keycodes via SYSCALL_PRINT_INT
  uint64_t size_keynotify =
      (uint64_t)user_keynotify_end - (uint64_t)user_keynotify_start;
  result_t ruser =
      proc_from_code(user_keynotify_start, size_keynotify, "ukeynotify");
  if (!result_is_ok(ruser)) {
    printf("Failed to start user keynotify process\n", PRINT_FLAG_BOTH);
  }

  virtio_register_all_drivers();
  printf("VirtIO: static bus scan...\n", PRINT_FLAG_BOTH);
  virtio_bus_init_static();

  virtio_block_dev_t *blk = virtio_blk_get();
  result_t rdisk = make_disk(blk);
  if (!result_is_ok(rdisk)) {
    dbg("make_disk(...) != OK");
    panic("Failed to create disk device");
  }
  disk_t *disk = (disk_t *)result_unwrap(rdisk);
  if (!disk_init(disk)) {
    dbg("disk_init(...) == false");
    panic("Failed to initialize disk device");
  }
  set_shared_disk(disk);

  fat_list_root(disk);

  sbi_set_timer(get_csrr_time() + TICK_INTERVAL_CYCLES);

  scheduler();

  panic("hi");
}
