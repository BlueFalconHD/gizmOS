#include "process_table.h"
#include "buddy_allocator.h"
#include "process.h"
#include <lib/log.h>
#include <lib/panic.h>
#include <lib/print.h>
static inline log_t *proc_tbl_log() {
  static log_t *l = NULL;
  if (!l)
    l = g_log_create("proc", "table");
  return l;
}
#include <lib/spinlock.h>
#include <mem_layout.h>
#include <page_table.h>

proc_t processes[NPROC];

uint64_t current_pid = 0;
struct spinlock current_pid_lock;

struct spinlock wait_lock;

proc_t *init_proc;

g_bool setup_process_kernel_stack(proc_t *p, uint8_t pidx) {
  if (!p)
    return false;

  if (!p->kstack) {
    uint64_t kstackvaddr = KSTACK(pidx);

    for (int i = 0; i < KSTACK_PAGES; i++) {
      void *kpage = buddy_alloc_page();
      if (!kpage) {
        panic_msg("Kernel stack allocation failed");
        LOG_ERROR(proc_tbl_log(), "pidx=%{type: int}", pidx);
        panic_loc("setup_process_kernel_stack");
      }

      uint64_t kstackpaddr = V2P((uint64_t)kpage);
      uint64_t vaddr = kstackvaddr + i * PAGE_SIZE;

      if (!map_page(shared_page_table, vaddr, kstackpaddr,
                    PTE_R | PTE_W | PTE_X | PTE_V)) {
        panic_msg("Kernel stack mapping failed");
        LOG_ERROR(proc_tbl_log(), "pidx=%{type: int}", pidx);
        panic_loc("setup_process_kernel_stack");
      }
    }

    p->kstack = kstackvaddr;
  }

  return true;
}

g_bool initialize_processes() {
  for (uint8_t i = 0; i < NPROC; i++) {
    proc_t *p = &processes[i];
    initlock(&p->lock, "proc");
    p->state = UNUSED;
    setup_process_kernel_stack(p, i);
  }

  // lock init
  initlock(&current_pid_lock, "pid_lock");
  initlock(&wait_lock, "wait");
  return true;
}

uint64_t allocate_pid() {
  uint64_t new_pid = 0;
  acquire(&current_pid_lock);
  new_pid = current_pid;
  current_pid++;
  release(&current_pid_lock);
  return new_pid;
}
