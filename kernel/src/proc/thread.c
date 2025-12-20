#include "thread.h"

#include "buddy_allocator.h"
#include "lifecycle.h"
#include "notification.h"
#include "process_table.h"
#include "scheduler.h"
#include "sleep.h"
#include <lib/cpu.h>
#include <lib/memory.h>
#include <lib/panic.h>
#include <lib/usermem.h>
#include <mem_layout.h>
#include <page_table.h>

extern void forkret();

static void free_thread_locked(proc_t *t) {
  if (!t) return;

  // Threads never own the address space; they only own their trapframe.
  if (t->trapframe) {
    buddy_free_page(t->trapframe);
    t->trapframe = NULL;
  }

  // Clear thread-local state. Keep kstack permanently allocated/mapped.
  t->sz = 0;
  t->pagetable = NULL;
  t->heap_base = 0;
  t->stack_base = 0;
  t->stack_top = 0;
  t->pid = 0;
  t->tg_leader = NULL;
  t->tgid = 0;
  t->is_thread = 0;
  __atomic_store_n(&t->tg_running_cpu, -1, __ATOMIC_RELAXED);
  t->parent = 0;
  t->name[0] = 0;
  t->chan = 0;
  t->killed = 0;
  t->xstate = 0;
  t->state = UNUSED;
}

RESULT_TYPE(proc_t *) make_thread(proc_t *leader_in) {
  proc_t *leader = proc_group(leader_in);
  if (!leader || leader->state == UNUSED)
    return RESULT_FAILURE(RESULT_INVALID_ARG);
  if (leader->is_kernel || !leader->pagetable)
    return RESULT_FAILURE(RESULT_INVALID_ARG);

  proc_t *t = NULL;
  for (uint8_t i = 0; i < NPROC; i++) {
    t = &processes[i];
    acquire(&t->lock);
    if (t->state == UNUSED) {
      goto found;
    }
    release(&t->lock);
  }
  return RESULT_FAILURE(RESULT_BUSY);

found:
  t->pid = allocate_pid();
  t->tg_leader = leader;
  t->tgid = (uint32_t)leader->pid;
  t->is_thread = 1;
  __atomic_store_n(&t->tg_running_cpu, -1, __ATOMIC_RELAXED);
  t->state = USED;
  t->priority = leader->priority;

  struct trapframe *tf = (struct trapframe *)buddy_alloc_page();
  if (!tf) {
    t->state = UNUSED;
    release(&t->lock);
    return RESULT_FAILURE(RESULT_NOMEM);
  }
  t->trapframe = tf;
  memset(t->trapframe, 0, sizeof(*t->trapframe));

  // Share the leader's address space.
  t->pagetable = leader->pagetable;
  t->sz = leader->sz;
  t->heap_base = leader->heap_base;
  t->stack_base = 0;
  t->stack_top = 0;

  memset(&t->context, 0, sizeof(context_t));
  t->context.ra = (uint64_t)forkret;
  t->context.sp = t->kstack + KSTACK_PAGES * PAGE_SIZE;

  return RESULT_SUCCESS(t); // returns with t->lock held (caller releases)
}

static g_bool is_user_mapped(proc_t *leader, uint64_t uva) {
  if (!leader || !leader->pagetable) return false;
  if (uva == 0 || uva >= TRAMPOLINE) return false;
  uint64_t pa = 0;
  return get_physical_address(leader->pagetable, uva, &pa);
}

uint64_t thread_create(uint64_t entry_va, uint64_t arg, uint64_t stack_top) {
  proc_t *caller = current_proc();
  proc_t *leader = proc_group(caller);

  if (!leader || leader->is_kernel) return (uint64_t)-1;
  if (!is_user_mapped(leader, entry_va)) return (uint64_t)-1;
  if (stack_top < 16) return (uint64_t)-1;
  if (!is_user_mapped(leader, stack_top - 1)) return (uint64_t)-1;

  // Ensure the shared user stubs exist (notification.done + thread.exit).
  if (!notification_ensure_userbuf(leader)) return (uint64_t)-1;

  result_t rt = make_thread(leader);
  if (!result_is_ok(rt)) return (uint64_t)-1;

  proc_t *t = (proc_t *)result_unwrap(rt);

  // Thread entry convention:
  //   a0 = arg
  //   epc = entry_va
  //   sp  = stack_top
  //   ra  = user stub that performs thread.exit(a0) when entry returns
  t->trapframe->epc = entry_va;
  t->trapframe->sp = stack_top;
  t->trapframe->a0 = arg;
  t->trapframe->ra = leader->notif_userbuf_base + THREAD_STUB_OFFSET;

  // Start runnable.
  t->state = RUNNABLE;
  uint64_t tid = (uint64_t)t->pid;
  release(&t->lock);
  return tid;
}

void thread_exit(uint64_t status) {
  proc_t *t = current_proc();
  proc_t *leader = proc_group(t);

  if (!t || !leader) panic("thread_exit: no proc");
  if (t == leader) {
    // Keep process semantics: exiting the leader exits the whole process.
    exit(status);
  }

  acquire(&wait_lock);
  acquire(&t->lock);
  t->xstate = (int)status;
  t->state = ZOMBIE;
  wakeup(t);
  release(&wait_lock);

  sched();
  panic("thread_exit: zombie returned");
}

uint64_t thread_join(uint64_t tid, uint64_t status_out_uva) {
  proc_t *caller = current_proc();
  proc_t *leader = proc_group(caller);

  if (!leader || leader->is_kernel) return (uint64_t)-1;
  if (tid == 0 || (int64_t)tid < 0) return (uint64_t)-1;
  if ((int)tid == leader->pid) return (uint64_t)-1;
  if ((int)tid == caller->pid) return (uint64_t)-1;

  acquire(&wait_lock);
  for (;;) {
    proc_t *t = NULL;
    for (uint8_t i = 0; i < NPROC; i++) {
      proc_t *p = &processes[i];
      acquire(&p->lock);
      if (p->state != UNUSED && p->is_thread && (uint64_t)p->pid == tid &&
          proc_group(p) == leader) {
        t = p; // keep t->lock held
        break;
      }
      release(&p->lock);
    }

    if (!t) {
      release(&wait_lock);
      return (uint64_t)-1;
    }

    if (t->state == ZOMBIE) {
      int st = t->xstate;
      if (status_out_uva != 0 &&
          !result_is_ok(copyout(leader->pagetable, status_out_uva, &st, sizeof(st)))) {
        release(&t->lock);
        release(&wait_lock);
        return (uint64_t)-1;
      }

      uint64_t rtid = (uint64_t)t->pid;
      free_thread_locked(t);
      release(&t->lock);
      release(&wait_lock);
      return rtid;
    }

    release(&t->lock);
    sleep((void *)t, &wait_lock);
  }
}

void thread_group_reap(proc_t *leader_in) {
  proc_t *leader = proc_group(leader_in);
  if (!leader) return;

  for (uint8_t i = 0; i < NPROC; i++) {
    proc_t *p = &processes[i];
    if (p == leader) continue;
    acquire(&p->lock);
    if (p->state != UNUSED && proc_group(p) == leader) {
      free_thread_locked(p);
    }
    release(&p->lock);
  }
}
