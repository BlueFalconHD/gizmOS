#include "lifecycle.h"
#include "lib/kalloc.h"
#include "buddy_allocator.h"
#include "memory.h"
#include "notification.h"
#include "process.h"
#include "process_table.h"
#include "scheduler.h"
#include "sleep.h"
#include <lib/cpu.h>
#include <lib/memory.h>
#include <lib/print.h>
#include <lib/str.h>
#include <lib/usermem.h>
#include <mem_layout.h>
#include <page_table.h>

#define PROC_LIFECYCLE_DEBUG_LEVEL 0

extern void forkret();

g_bool killed(proc_t *p) {
  g_bool is_killed = false;
  acquire(&p->lock);
  if (p->killed) {
    is_killed = true;
  }
  release(&p->lock);
  return is_killed;
}

RESULT_TYPE(proc_t *) make_proc() {
  proc_t *p = NULL;

  for (uint8_t i = 0; i < NPROC; i++) {
    p = &processes[i];
    acquire(&p->lock);
    if (p->state == UNUSED) {
      goto found;
    } else {
      release(&p->lock);
    }
  }

  return RESULT_FAILURE(RESULT_BUSY);

found:

  p->pid = allocate_pid();
  p->state = USED;
  p->priority = PROC_PRIORITY_NORMAL;

  struct trapframe *tf = (struct trapframe *)buddy_alloc_page();
  if (!tf) {
    release(&p->lock);
    return RESULT_FAILURE(RESULT_NOMEM);
  }
  p->trapframe = tf;

  page_table_t *pt = allocate_process_page_table(p);
  if (!pt) {
    kfree(p->trapframe);
    p->trapframe = NULL;
    release(&p->lock);
    return RESULT_FAILURE(RESULT_NOMEM);
  }

  p->pagetable = pt;

  memset(&p->context, 0, sizeof(context_t));
  p->context.ra = (uint64_t)forkret;
  p->context.sp = p->kstack + KSTACK_PAGES * PAGE_SIZE;

  // Initialize notification subsystem for this process
  notification_init_proc(p);

#if PROC_LIFECYCLE_DEBUG_LEVEL >= 1
  printf("alloc proc kstack = %{type: hex}\n", PRINT_FLAG_BOTH, p->kstack);
#endif

  return RESULT_SUCCESS(p);
}

void free_process(proc_t *p) {
  if (!p)
    return;

  if (p->pagetable) {
    buddy_free_page(p->pagetable);
    p->pagetable = NULL;
  }

  if (p->trapframe) {
    buddy_free_page(p->trapframe);
    p->trapframe = NULL;
  }

  p->sz = 0;
  p->pid = 0;
  p->parent = 0;
  p->name[0] = 0;
  p->chan = 0;
  p->killed = 0;
  p->xstate = 0;

  p->state = UNUSED;
}

void reparent(proc_t *p) {
  for (uint64_t i = 0; i < NPROC; i++) {
    proc_t *child = &processes[i];
    if (child->parent == p) {
      child->parent = init_proc;
      wakeup(init_proc);
    }
  }
}

void exit(uint64_t status) {
  proc_t *p = current_proc();

  if (p == init_proc)
    panic("init proc exiting");

  acquire(&wait_lock);

  reparent(p);
  wakeup(p->parent);

  acquire(&p->lock);
  p->xstate = status;
  p->state = ZOMBIE;

  release(&wait_lock);

  sched();
  panic("zombie exit");
}

uint64_t wait(uint64_t address) {
  proc_t *pp;
  g_bool has_children = false;
  uint64_t pid;
  proc_t *p = current_proc();

#if PROC_LIFECYCLE_DEBUG_LEVEL >= 3
  printf("proc %{type: int} (%s) entering wait\n", PRINT_FLAG_BOTH, p->pid,
         p->name);
#endif

  acquire(&wait_lock);

  for (;;) {
    has_children = 0;

    for (uint8_t i = 0; i < NPROC; i++) {
      pp = &processes[i];
      if (pp->parent == p) {
#if PROC_LIFECYCLE_DEBUG_LEVEL >= 3
        printf("proc %{type: int} (%s) found child proc %{type: int} (%s) in "
               "state %{type: int}\n",
               PRINT_FLAG_BOTH, p->pid, p->name, pp->pid, pp->name, pp->state);
#endif

        acquire(&pp->lock);
        has_children = 1;

        if (pp->state == ZOMBIE) {
#if PROC_LIFECYCLE_DEBUG_LEVEL >= 2
          printf(
              "proc %{type: int} (%s) reaping child proc %{type: int} (%s)\n",
              PRINT_FLAG_BOTH, p->pid, p->name, pp->pid, pp->name);
#endif

          pid = pp->pid;
          if (address != 0 &&
              !result_is_ok(copyout(p->pagetable, address, (void *)&p->xstate,
                                    sizeof(p->xstate)))) {
            release(&pp->lock);
            release(&wait_lock);
            return -1;
          }

          free_process(pp);
          release(&pp->lock);
          release(&wait_lock);
          return pid;
        }

        release(&pp->lock);
      }

      if (!has_children || killed(p)) {
#if PROC_LIFECYCLE_DEBUG_LEVEL >= 1
        if (!has_children) {
          printf("proc %{type: int} (%s) has no children\n", PRINT_FLAG _BOTH,
                 p->pid, p->name);
        }

        if (killed(p)) {
          printf("proc %{type: int} (%s) was killed\n", PRINT_FLAG_BOTH, p->pid,
                 p->name);
        }
#endif

        release(&wait_lock);
        return -1;
      }

      sleep(p, &wait_lock);
    }
  }
}

RESULT_TYPE(void) kill(uint64_t pid) {
  proc_t *p;

  for (uint8_t i = 0; i < NPROC; i++) {
    p = &processes[i];
    acquire(&p->lock);
    if ((uint64_t)p->pid == pid) {
      p->killed = 1;
      if (p->state == SLEEPING) {
        p->state = RUNNABLE;
      }
      release(&p->lock);
      return RESULT_SUCCESS(0);
    }
    release(&p->lock);
  }

  return RESULT_FAILURE(RESULT_NOT_FOUND);
}

void setkilled(proc_t *p) {
  acquire(&p->lock);
  p->killed = 1;
  release(&p->lock);
}

uint64_t fork(void) {
  uint64_t pid;

  proc_t *p = current_proc();

  result_t rnew_proc = make_proc();
  if (!result_is_ok(rnew_proc)) {
    return -1;
  }

  proc_t *new_proc = (proc_t *)result_unwrap(rnew_proc);

  if (!uvmcopy(p->pagetable, new_proc->pagetable, p->sz)) {
    free_process(new_proc);
    return -1;
  }

  new_proc->sz = p->sz;

  *(new_proc->trapframe) = *(p->trapframe);
  new_proc->trapframe->a0 = 0;

  pid = new_proc->pid;

  release(&new_proc->lock);

  acquire(&wait_lock);
  new_proc->parent = p;
  release(&wait_lock);

  acquire(&new_proc->lock);
  new_proc->state = RUNNABLE;
  release(&new_proc->lock);

  return pid;
}

RESULT_TYPE(proc_t *)
proc_from_code(uint8_t code[], uint64_t size, const char *name) {
  proc_t *p = NULL;

  result_t rp = make_proc();
  if (!result_is_ok(rp)) {
    return RESULT_FAILURE(RESULT_NOMEM);
  }

  p = (proc_t *)result_unwrap(rp);

  // Allocate user memory up to size and copy code to VA=0
  uint64_t newsz = ((size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1));
  if (!uvmalloc(p, 0, newsz)) {
    free_process(p);
    release(&p->lock);
    return RESULT_FAILURE(RESULT_NOMEM);
  }

  // Copy program bytes into mapped region
  uint64_t remaining = size;
  uint64_t offset = 0;
  while (remaining > 0) {
    uint64_t chunk = remaining;
    // copyout copies from kernel buffer to user VA space
    if (!result_is_ok(copyout(p->pagetable, offset, code + offset, chunk))) {
      uvmdealloc(p, newsz, 0);
      free_process(p);
      release(&p->lock);
      return RESULT_FAILURE(RESULT_ERROR);
    }
    offset += chunk;
    remaining -= chunk;
  }

  p->sz = newsz;

  // Set initial trapframe for user entry
  p->trapframe->epc = 0;    // entry point at 0
  p->trapframe->sp = newsz; // simple stack at top of image

  if (name != NULL)
    strncopy(p->name, name, sizeof(p->name));

  p->state = RUNNABLE;
  release(&p->lock);
  return RESULT_SUCCESS(p);
}
