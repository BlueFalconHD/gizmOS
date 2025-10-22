#include "user_trap.h"
#include "lifecycle.h"
#include "notification.h"
#include "proc/syscall.h"
#include "process.h"
#include "scheduler.h"
#include <lib/cpu.h>
#include <lib/gfx.h>
#include <lib/gizm_font.h>
#include <lib/kalloc.h>
#include <lib/log.h>
#include <lib/panic.h>
#include <lib/print.h>
#include <lib/usermem.h>
#include <mem_layout.h>
#include <page_table.h>
#include <platform/interrupts.h>
#include <platform/registers.h>
#include <syscall.h>
// exit() is declared in lifecycle.h; keep implicit through user_trap.c's
// existing includes

// Debugging for notification injection into user trap return
#ifndef NOTIF_DELIVERY_DEBUG_LEVEL
#define NOTIF_DELIVERY_DEBUG_LEVEL 0
#endif

static inline log_t *user_trap_log() {
  static log_t *l = NULL;
  if (!l)
    l = g_log_create("trap", "user");
  return l;
}

extern char trampoline[];
extern char uservec[];
extern char userret[];
extern void trap_vector();
extern uint64_t hhdm_offset;

void user_trap_ret(void) {
  proc_t *p = current_proc();
  PS_disable_interrupts();

  if (p->is_kernel) {
    PS_set_trap_vector((uint64_t)trap_vector);
    PS_enable_interrupts();
    return;
  }

  // Before switching back to user, inject a pending notification if any.
  if (!p->is_kernel && p->notif_pending && p->notif_ctx.valid == 0) {
    notif_msg_t m;
    if (notification_pop(p, &m)) {
      if (notification_ensure_userbuf(p)) {
        notif_handler_t *h = &p->notif_handlers[m.type];
        if (h->handler_va != 0) {
          uint64_t uva = p->notif_userbuf_base + NOTIF_PAYLOAD_OFFSET;
          uint64_t maxn = p->notif_userbuf_size - NOTIF_PAYLOAD_OFFSET;
          uint64_t n = (m.len < maxn) ? m.len : maxn;
          if (!result_is_ok(copyout(p->pagetable, uva, m.kbuf, n))) {
            // failed copy; drop
            n = 0;
          }
          if (n < m.len) {
            m.flags |= NOTIF_DFLAG_TRUNCATED;
          }

#if NOTIF_DELIVERY_DEBUG_LEVEL >= 1
          LOG_DEBUG(user_trap_log(),
                    "[notif] inject: pid=%{type: int} type=%{type: int} "
                    "n=%{type: int} handler=%{type: hex}",
                    p->pid, (int)m.type, (int)n, h->handler_va);
#endif
          notif_ctx_save_from_trapframe(p);
          p->trapframe->a0 = m.type;
          p->trapframe->a1 = uva;
          p->trapframe->a2 = n;
          p->trapframe->a3 = h->arg_va;
          // set a7 so user handler can ecall NOTIF_DONE and return
          p->trapframe->a7 = SYSCALL_NOTIF_DONE;
          p->trapframe->epc = h->handler_va;
        }
      }
      if (m.kbuf)
        kfree(m.kbuf);
    }
  }

  uint64_t trampoline_uservec = TRAMPOLINE + (uservec - trampoline);
  PS_set_trap_vector(trampoline_uservec);

  p->trapframe->kernel_satp = PS_get_atp();
  p->trapframe->kernel_sp = p->kstack + KSTACK_PAGES * PAGE_SIZE;
  p->trapframe->kernel_trap = (uint64_t)usertrap;
  p->trapframe->kernel_hartid = P_get_thread_ptr();

  uint64_t x = PS_get_status();
  x &= ~SSTATUS_SPP;
  x |= SSTATUS_SPIE;
  PS_set_status(x);

  PS_set_exception_pc(p->trapframe->epc);

  uint64_t table_pa = ((uint64_t)p->pagetable) - hhdm_offset;
  uint64_t table_ppn = table_pa >> 12;
  uint64_t satp_value = (uint64_t)8ULL << 60;
  satp_value |= table_ppn;

  uint64_t trampoline_userret = TRAMPOLINE + (userret - trampoline);
  ((void (*)(uint64_t))trampoline_userret)(satp_value);
}

void forkret() {
  release(&current_proc()->lock);
  user_trap_ret();
}

void usertrap(void) {
  if ((PS_get_status() & SSTATUS_SPP) != 0)
    panic("usertrap: not from user mode");

  PS_set_trap_vector((uint64_t)trap_vector);

  proc_t *p = current_proc();

  if (PS_get_exception_cause() == 0x2) {
    uint64_t faulting_address = PS_get_exception_pc();
    uint64_t fault_pa = 0;

    if (get_physical_address(p->pagetable, faulting_address, &fault_pa)) {
      uint64_t fault_va = fault_pa + hhdm_offset;
      LOG_ERROR(user_trap_log(), "Faulting address: %{type: hex}", fault_va);
      LOG_ERROR(user_trap_log(), "Data at faulting address: 0x%{type: hex}",
                *(uint64_t *)fault_va);
    } else {
      LOG_ERROR(user_trap_log(),
                "Failed to get physical address for faulting address");
    }
  }

  p->trapframe->epc = PS_get_exception_pc();

  if (PS_get_exception_cause() == 8) {
    // Advance past the ecall so we don't re-trap on the same instruction
    p->trapframe->epc += 4;
    PS_enable_interrupts();

    int callnum = p->trapframe->a7;
    syscall_err_t e = syscall_dispatch(p, callnum);
    p->trapframe->a7 = (uint64_t)e;
    goto out;
  }

  if (PS_get_exception_cause() == 0x8000000000000005) {
    yield();
  }
out:
  user_trap_ret();
}
