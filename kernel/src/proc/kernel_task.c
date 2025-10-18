#include "kernel_task.h"
#include "lib/kalloc.h"
#include "buddy_allocator.h"
#include "lifecycle.h"
#include "scheduler.h"
#include <lib/cpu.h>
#include <lib/panic.h>
#include <lib/spinlock.h>
#include <lib/str.h>
#include <mem_layout.h>

void kernel_task_wrapper(void) {
  proc_t *p = current_proc();
  void (*real_entry)(void *) = (void (*)(void *))p->context.s0;
  void *arg = (void *)p->context.s1;

  release(&p->lock);
  real_entry(arg);

  acquire(&p->lock);
  p->state = ZOMBIE;
  sched();
  panic("kernel task returned from sched unexpectedly");
}

RESULT_TYPE(proc_t *)
make_kernel_task(void (*entry)(void *), void *arg, const char *name) {
  result_t r = make_proc();
  if (!result_is_ok(r))
    return RESULT_FAILURE(RESULT_NOMEM);

  proc_t *p = (proc_t *)result_unwrap(r);

  kfree(p->trapframe);
  p->trapframe = NULL;

  buddy_free_page(p->pagetable);
  p->pagetable = shared_page_table;

  p->is_kernel = 1;
  p->context.ra = (uint64_t)kernel_task_wrapper;
  p->context.sp = p->kstack + KSTACK_PAGES * PAGE_SIZE;
  p->context.s0 = (uint64_t)entry;
  p->context.s1 = (uint64_t)arg;

  strncopy(p->name, name, sizeof(p->name));

  if (name && strcmp(name, "framebufferd")) {
    p->priority = PROC_PRIORITY_FLUSH;
  } else {
    p->priority = PROC_PRIORITY_HIGH;
  }

  p->state = RUNNABLE;
  release(&p->lock);
  return RESULT_SUCCESS(p);
}
