#include "earlyinit.h"
// #include "kprocs/tetris.h"
// #include "lib/canary.h"
// #include "lib/dyn_array.h"
// #include "lib/macros.h"
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
#include <fs/objectfs/objfs.h>
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

// debug level for boot logs:
//   0 -> info and above (quieter)
//   1 -> include debug boot logs (verbose)
#ifndef KERN_BOOT_DEBUG_LEVEL
#define KERN_BOOT_DEBUG_LEVEL 0
#endif

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

typedef struct {
  log_t *log;
} list_ctx_t;
static void objfs_root_emit(const char *name, uint8_t kind, uint64_t id, void *arg) {
  (void)kind; (void)id;
  list_ctx_t *ctx = (list_ctx_t *)arg;
  char line[128];
  int pos = 0;
  const char *t = "[obj]";
  for (int i = 0; t[i] && pos + 1 < (int)sizeof(line); i++) line[pos++] = t[i];
  line[pos++] = ' ';
  for (int i = 0; name[i] && pos + 1 < (int)sizeof(line); i++) line[pos++] = name[i];
  line[pos] = '\0';
  LOG_DEBUG(ctx->log, "%{type: str}", line);
}
static void objfs_list_root_once(log_t *log) {
  uint64_t root = objfs_global()->sb.root_object_id;
  list_ctx_t ctx = {.log = log};
  objfs_list_subobjects(root, objfs_root_emit, &ctx);
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
  #if KERN_BOOT_DEBUG_LEVEL >= 1
  g_log_set_level(kern_log, LOG_LEVEL_DEBUG);
  #else
  g_log_set_level(kern_log, LOG_LEVEL_INFO);
  #endif

  LOG_INFO(kern_log, "gizmOS %{type: str}", VERSION);
  LOG_INFO(kern_log, "gizmos kernel %{type: str} starting up", VERSION);

  LOG_DEBUG(kern_log, "setting up mmio mapping");

  mmio_map *mmap = alloc_mmio_map();

  LOG_DEBUG(kern_log, "mapping mmio for uart");
  mmio_map_add(mmap, 0x10000000, 0x1000, PTE_R | PTE_W | PTE_X | PTE_V,
               1); // UART

  LOG_DEBUG(kern_log, "mapping mmio for rtc");
  mmio_map_add(mmap, 0x101000, 0x1000, PTE_R | PTE_W | PTE_X | PTE_V,
               1); // RTC

  LOG_DEBUG(kern_log, "mapping mmio for plic");
  mmio_map_add(mmap, 0x0C000000, 0x00600000, PTE_R | PTE_W | PTE_X | PTE_V,
               1); // PLIC

  LOG_DEBUG(kern_log, "mapping mmio for virtio devices");
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

  LOG_INFO(kern_log, "mmio mapping ready");
  activate_page_table(shared_page_table);

  LOG_DEBUG(kern_log, "setting up trampoline mapping");
  bool success =
      map_page(shared_page_table, TRAMPOLINE, V2P((uint64_t)trampoline),
               PTE_R | PTE_W | PTE_X | PTE_V);
  if (!success) {
    dbg("map_page(...) == false");
    panic("Failed to set up trampoline mapping");
  }

  LOG_DEBUG(kern_log, "trampoline mapping ready");
  activate_page_table(shared_page_table);

  LOG_INFO(kern_log, "bringing devices online");

  LOG_DEBUG(kern_log, "initializing uart");

  result_t ruart = make_uart(0x10000000);
  if (!result_is_ok(ruart)) {
    dbg("make_uart(...) != OK");
    panic("Failed to create UART");
  }
  uart_t *uart = (uart_t *)result_unwrap(ruart);
  if (!uart_init(uart)) {
    dbg("uart_init(...) == false");
    panic("Failed to initialize uart");
  }
  set_shared_uart(uart);
  LOG_DEBUG(kern_log, "uart ready");

  // before anything bad can happen, print framebuffer memory info
  LOG_DEBUG(kern_log, "framebuffer address: 0x%{type: hex}", lfb->address);
  LOG_DEBUG(kern_log, "framebuffer pitch: %{type: int}", lfb->pitch);
  LOG_DEBUG(kern_log, "framebuffer width: %{type: int}", lfb->width);
  LOG_DEBUG(kern_log, "framebuffer height: %{type: int}", lfb->height);
  LOG_DEBUG(kern_log, "framebuffer bpp: %{type: int}", lfb->bpp);
  LOG_DEBUG(kern_log, "framebuffer red mask size: %{type: int}",
            lfb->red_mask_size);

  LOG_DEBUG(kern_log, "initializing rtc");
  result_t rrtc = make_rtc(0x101000);
  if (!result_is_ok(rrtc)) {
    dbg("make_rtc(...) != OK");
    panic("Failed to create rtc");
  }
  rtc_t *rtc = (rtc_t *)result_unwrap(rrtc);
  if (!rtc_init(rtc)) {
    dbg("rtc_init(...) == false");
    panic("Failed to initialize rtc");
  }
  set_shared_rtc(rtc);
  LOG_DEBUG(kern_log, "rtc ready");

  LOG_DEBUG(kern_log, "initializing plic");
  result_t rplic = make_plic(0x0C000000);
  plic_t *plic = (plic_t *)result_unwrap(rplic);
  if (!plic_init(plic)) {
    dbg("plic_init(...) == false");
    panic("Failed to initialize plic");
  }
  set_shared_plic(plic);
  LOG_DEBUG(kern_log, "plic ready");

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
  LOG_DEBUG(kern_log, "cursor ready");
  LOG_INFO(kern_log, "devices ready");

  LOG_DEBUG(kern_log, "configuring plic interrupts");

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

  LOG_INFO(kern_log, "plic interrupt routing ready");

  LOG_DEBUG(kern_log, "enabling cpu interrupts");

  enable_interrupts();

  LOG_INFO(kern_log, "cpu interrupts enabled");

  uart_enable_interrupts(uart);

  LOG_DEBUG(kern_log, "uart rx/tx interrupts armed");

  LOG_INFO(kern_log, "process system coming up");

  initialize_processes();

  LOG_DEBUG(kern_log, "initializing virtio bus and drivers");

  virtio_register_all_drivers();
  LOG_DEBUG(kern_log, "virtio: static bus scan...");
  virtio_bus_init_static();

  LOG_INFO(kern_log, "virtio bus ready");

  LOG_DEBUG(kern_log, "initializing disk device");

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

  LOG_INFO(kern_log, "disk device ready");

  // Mount ObjectFS as root and list directory once
  result_t rmnt = objfs_mount_root(disk);
  if (!result_is_ok(rmnt)) {
    LOG_WARN(kern_log, "objectfs mount failed (no legacy FAT fallback)");
  } else {
    LOG_INFO(kern_log, "objectfs mounted as root fs");
    objfs_list_root_once(kern_log);
  }

  // // Attempt to start a Vessel user program from the FAT root
  // result_t rvesselh = proc_from_vessel_path("HELLO.VES", "hello");
  // if (!result_is_ok(rvesselh)) {
  //   LOG_WARN(kern_log, "Failed to start hello.vessel (HELLO.VES)");
  // }

  // // Attempt to start a Vessel user program from the FAT root
  // result_t rvesself = proc_from_vessel_path("FIZZBUZ.VES", "fizzbuzz");
  // if (!result_is_ok(rvesself)) {
  //   LOG_WARN(kern_log, "Failed to start fizzbuzz.vessel (FIZZBUZ.VES)");
  // }

  // result_t rvesselk = proc_from_vessel_path("KEYNOTFY.VES", "keynotify");
  // if (!result_is_ok(rvesselk)) {
  //   LOG_WARN(kern_log, "failed to start keynotfy.vessel (KEYNOTFY.VES)");
  // }

  result_t rshk = proc_from_vessel_path("sh.vessel", "sh");
  if (!result_is_ok(rshk)) {
    LOG_WARN(kern_log, "couldn't start sh.vessel (sh.ves)");
  }

  LOG_INFO(kern_log, "starting scheduler, hands off now");

  sbi_set_timer(get_csrr_time() + TICK_INTERVAL_CYCLES);

  scheduler();

  panic("hi");
}

EARLY_TEXT void main() {
  early_init_status eastat = early_init();
  (void)eastat;

  realmain();
}
