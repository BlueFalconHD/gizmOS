#include "earlyinit.h"
// #include "kprocs/tetris.h"
// #include "lib/canary.h"
// #include "lib/dyn_array.h"
// #include "lib/macros.h"
#include "img/ppm_fs.h"
#include "lib/debug.h"
#include "lib/log.h"
#include "lib/panic.h"
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

/* init_trap_vector is no longer used */

extern uint8_t proc_ecall7_start[];
extern uint8_t proc_ecall7_end[];
extern uint8_t proc_ecall8_start[];
extern uint8_t proc_ecall8_end[];
extern uint8_t user_keynotify_start[];
extern uint8_t user_keynotify_end[];

/*
 * Basic integer-only RNG for kernel main, no floating point.
 * Linear Congruential Generator (LCG): 64-bit state, 32-bit output.
 */
static uint64_t g_rand_state = 0x9e3779b97f4a7c15ULL; /* non-zero default */
static inline void rand_seed(uint64_t seed) {
  if (seed)
    g_rand_state ^= seed;
}
static inline uint32_t rand(void) {
  g_rand_state = g_rand_state * 6364136223846793005ULL + 1ULL;
  return (uint32_t)(g_rand_state >> 32);
}

void realmain() {
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

  log_t *kern_log = g_log_create("boot", NULL);
  LOG_INFO(kern_log, "gizmOS %{type: str}", VERSION);

  LOG_INFO(kern_log, "initializing memory mapped IO");

  mmio_map *mmap = alloc_mmio_map();

  LOG_DEBUG(kern_log, "mapping uart");
  mmio_map_add(mmap, 0x10000000, 0x1000, PTE_R | PTE_W | PTE_X | PTE_V,
               1); // UART

  LOG_DEBUG(kern_log, "mapping rtc");
  mmio_map_add(mmap, 0x101000, 0x1000, PTE_R | PTE_W | PTE_X | PTE_V,
               1); // RTC

  LOG_DEBUG(kern_log, "mapping plic");
  mmio_map_add(mmap, 0x0C000000, 0x00600000, PTE_R | PTE_W | PTE_X | PTE_V,
               1); // PLIC

  LOG_DEBUG(kern_log, "mapping virtio devices");
  mmio_map_add(mmap, 0x10001000, 0x1000, PTE_R | PTE_W | PTE_X | PTE_V,
               1); // Virtio keyboard
  mmio_map_add(mmap, 0x10002000, 0x1000, PTE_R | PTE_W | PTE_X | PTE_V,
               1); // Virtio mouse
  mmio_map_add(mmap, 0x10003000, 0x1000, PTE_R | PTE_W | PTE_X | PTE_V,
               1); // Virtio gpu
  mmio_map_add(mmap, 0x10004000, 0x1000, PTE_R | PTE_W | PTE_X | PTE_V,
               1); // Virtio block

  LOG_DEBUG(kern_log, "applying mmio map to page table");
  mmio_map_pages(mmap, shared_page_table);

  LOG_INFO(kern_log, "memory mapped IO initialized");
  activate_page_table(shared_page_table);

  LOG_DEBUG(kern_log, "setting up trampoline mapping");
  bool success =
      map_page(shared_page_table, TRAMPOLINE, V2P((uint64_t)trampoline),
               PTE_R | PTE_W | PTE_X | PTE_V);
  if (!success) {
    dbg("map_page(...) == false");
    panic("Failed to set up trampoline mapping");
  }

  LOG_INFO(kern_log, "trampoline mapping set up");
  activate_page_table(shared_page_table);

  LOG_INFO(kern_log, "initializing devices");

  LOG_DEBUG(kern_log, "initializing uart");

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
  LOG_INFO(kern_log, "UART initialized");

  // before anything bad can happen, print framebuffer memory info
  LOG_DEBUG(kern_log, "Framebuffer address: 0x%{type: hex}", lfb->address);
  LOG_DEBUG(kern_log, "Framebuffer pitch: %{type: int}", lfb->pitch);
  LOG_DEBUG(kern_log, "Framebuffer width: %{type: int}", lfb->width);
  LOG_DEBUG(kern_log, "Framebuffer height: %{type: int}", lfb->height);
  LOG_DEBUG(kern_log, "Framebuffer bpp: %{type: int}", lfb->bpp);
  LOG_DEBUG(kern_log, "Framebuffer red mask size: %{type: int}",
            lfb->red_mask_size);

  LOG_DEBUG(kern_log, "initializing RTC");
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
  LOG_INFO(kern_log, "RTC initialized");

  LOG_DEBUG(kern_log, "initializing PLIC");
  result_t rplic = make_plic(0x0C000000);
  plic_t *plic = (plic_t *)result_unwrap(rplic);
  if (!plic_init(plic)) {
    dbg("plic_init(...) == false");
    panic("Failed to initialize PLIC");
  }
  set_shared_plic(plic);
  LOG_INFO(kern_log, "PLIC initialized");

  LOG_DEBUG(kern_log, "initializing cursor");
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
  LOG_INFO(kern_log, "Cursor initialized");
  LOG_INFO(kern_log, "devices initialized");

  LOG_INFO(kern_log, "setting up PLIC");

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

  LOG_INFO(kern_log, "PLIC setup complete");

  LOG_INFO(kern_log, "initializing interrupts");

  enable_interrupts();

  LOG_INFO(kern_log, "interrupts initialized");

  uart_enable_interrupts(uart);

  LOG_DEBUG(kern_log, "uart is ready now");

  LOG_INFO(kern_log, "initializing processes");

  initialize_processes();

  LOG_INFO(kern_log, "initializing VirtIO bus and drivers");

  virtio_register_all_drivers();
  LOG_INFO(kern_log, "VirtIO: static bus scan...");
  virtio_bus_init_static();

  LOG_INFO(kern_log, "VirtIO bus and drivers initialized");

  LOG_INFO(kern_log, "initializing disk device");

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

  LOG_INFO(kern_log, "disk device initialized");

  fat_list_root(disk);

  // Attempt to start a Vessel user program from the FAT root
  result_t rvesselh = proc_from_vessel_path("HELLO.VES", "hello");
  if (!result_is_ok(rvesselh)) {
    LOG_WARN(kern_log, "Failed to start hello.vessel (HELLO.VES)");
  }

  result_t rvesselk = proc_from_vessel_path("KEYNOTFY.VES", "keynotify");
  if (!result_is_ok(rvesselk)) {
    LOG_WARN(kern_log, "Failed to start keynotfy.vessel (KEYNOTFY.VES)");
  }

  LOG_INFO(kern_log, "starting scheduler");

  sbi_set_timer(get_csrr_time() + TICK_INTERVAL_CYCLES);

  scheduler();

  panic("hi");
}

EARLY_TEXT void main() {
  early_init_status eastat = early_init();
  (void)eastat;

  realmain();
}
