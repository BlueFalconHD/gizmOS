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
#include <platform/tlb.h>
#include <syscall.h>
#include <lib/memory.h>
// exit() is declared in lifecycle.h; keep implicit through user_trap.c's
// existing includes

// Debugging for notification injection into user trap return
#ifndef NOTIF_DELIVERY_DEBUG_LEVEL
#define NOTIF_DELIVERY_DEBUG_LEVEL 0
#endif

static inline log_t *user_trap_log() {
  static log_t *l = NULL;
  if (!l) {
    l = g_log_create("trap", "user");
    #if NOTIF_DELIVERY_DEBUG_LEVEL >= 1
    g_log_set_level(l, LOG_LEVEL_DEBUG);
    #else
    g_log_set_level(l, LOG_LEVEL_INFO);
    #endif
  }
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

  proc_t *owner = proc_group(p);

  // Before switching back to user, inject a pending notification if any.
  // Allow limited nesting: when already in a handler (valid!=0), we can push
  // the current context and inject another handler as long as the stack
  // depth limit has not been reached.
  if (!p->is_kernel && owner && owner->notif_pending &&
      (p->notif_ctx.valid == 0 || notif_ctx_can_nest(p))) {
    notif_msg_t m;
    if (notification_pop(p, &m)) {
      if (notification_ensure_userbuf(owner)) {
        notif_handler_t *h = &owner->notif_handlers[m.type];
        if (h->handler_va != 0) {
          uint64_t uva = owner->notif_userbuf_base + NOTIF_PAYLOAD_OFFSET;
          uint64_t maxn = owner->notif_userbuf_size - NOTIF_PAYLOAD_OFFSET;
          uint64_t n = (m.len < maxn) ? m.len : maxn;
          if (!result_is_ok(copyout(owner->pagetable, uva, m.kbuf, n))) {
            // failed copy; drop
            n = 0;
          }
          if (n < m.len) {
            m.flags |= NOTIF_DFLAG_TRUNCATED;
          }

#if NOTIF_DELIVERY_DEBUG_LEVEL >= 1
          // print notification info (origin process, recipient process, type, length)
          LOG_DEBUG(user_trap_log(),
                    "notif.deliver: -> pid=%{type: int} name=%{type: str} type=%{type: int} len=%{type: int} (handler=0x%{type: hex}) (head=%{type: int} tail=%{type: int} valid=%{type: int} depth=%{type: int} pending=%{type: int})",
                    p->pid, p->name, (int)m.type, (int)m.len, h->handler_va,
                    (int)owner->notif_q_head, (int)owner->notif_q_tail, (int)p->notif_ctx.valid, (int)p->notif_stack.depth, (int)owner->notif_pending);
#endif
          /*
           * Save/stack the current user context so we can resume it once the
           * notification handler completes. In addition to the epc and
           * argument registers, we also snapshot the return address (ra).
           * If already handling a notification, push onto the nest stack.
           */
          if (p->notif_ctx.valid == 0) {
            notif_ctx_save_from_trapframe(p);
          } else {
            notif_ctx_push_from_trapframe(p);
          }
          /*
           * Arrange for the handler to be called as:
           *   handler(type, payload_uva, len, arg);
           * and for a plain `ret` from the handler to automatically
           * perform notification completion via the stub at NOTIF_STUB.
           *
           * We achieve this by:
           *   - setting ra to the user stub address (an ecall instruction)
           *   - jumping to the handler by setting epc to handler_va
           *
           * The stub will invoke SYSCALL_NOTIF_DONE, which restores the
           * saved context and resumes the interrupted user code.
           */
          p->trapframe->a0 = m.type;
          p->trapframe->a1 = uva;
          p->trapframe->a2 = n;
          p->trapframe->a3 = h->arg_va;
          p->trapframe->ra = owner->notif_userbuf_base + NOTIF_STUB_OFFSET;
          p->trapframe->epc = h->handler_va;
        }
      }
      if (m.kbuf)
        kfree(m.kbuf);
    }
  } else if (!p->is_kernel && owner && owner->notif_pending && p->notif_ctx.valid != 0) {
#if NOTIF_DELIVERY_DEBUG_LEVEL >= 1
    LOG_DEBUG(user_trap_log(),
             "[notif] skip(nested): pid=%{type: int} valid=%{type: int} "
             "(pending=%{type: int} head=%{type: int} tail=%{type: int})",
             p->pid, (int)p->notif_ctx.valid,
             (int)owner->notif_pending, (int)owner->notif_q_head, (int)owner->notif_q_tail);
#endif
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

  // For thread groups, all threads share a single user page table; the
  // trampoline expects TRAPFRAME to point to the currently-running thread's
  // trapframe page.
  if (!p->is_kernel && p->pagetable && p->trapframe) {
    (void)map_page(p->pagetable, TRAPFRAME, V2P((uint64_t)p->trapframe),
                   PTE_R | PTE_W | PTE_X | PTE_V);
    tlb_flush_all();
  }

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

  uint64_t scause = PS_get_exception_cause();
  uint64_t sepc = PS_get_exception_pc();
  uint64_t stval = PS_get_exception_value();

  // Decode cause into interrupt/exception + code
  uint64_t is_interrupt = (scause >> 63) & 1;
  uint64_t cause_code = scause & 0x7FFFFFFFFFFFFFFFULL;

  // Minimal per-process logging for user-mode faults (non-ecall exceptions).
  // This helps debug issues like missing user mappings or bad accesses without
  // halting the whole kernel via the global trap handler.
  if (!is_interrupt && cause_code != 8) {
    LOG_ERROR(user_trap_log(),
              "usertrap: pid=%{type: int} name=%{type: str} exception code=%{type: int} pc=0x%{type: hex} stval=0x%{type: hex}",
              p->pid, p->name, (int)cause_code, sepc, stval);

    // Immediately terminate the offending process, similar to Linux "killed by signal"
    // semantics for fatal user-space faults.
    LOG_ERROR(user_trap_log(),
              "usertrap: killing pid=%{type: int} name=%{type: str} due to fatal user exception code=%{type: int}",
              p->pid, p->name, (int)cause_code);

    // Exit with a status that encodes "killed by exception"; for now just use
    // the cause code as the exit status.
    exit(cause_code);
  }

  p->trapframe->epc = sepc;

  if (cause_code == 8 && !is_interrupt) {
    // Advance past the ecall so we don't re-trap on the same instruction
    p->trapframe->epc += 4;
    PS_enable_interrupts();

    int callnum = p->trapframe->a7;

#if SYSCALL_DEBUG_LEVEL >= 10
    LOG_DEBUG(user_trap_log(),
             "ecall: pid=%{type: int} num=0x%{type: hex}",
             p->pid, (uint64_t)callnum);
#endif

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
