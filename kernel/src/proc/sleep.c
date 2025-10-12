#include "sleep.h"
#include "process.h"
#include "process_table.h"
#include "scheduler.h"
#include <lib/cpu.h>

void wakeup(void *chan) {
  for (uint8_t i = 0; i < NPROC; i++) {
    proc_t *p = &processes[i];
    acquire(&p->lock);
    if (p->state == SLEEPING && p->chan == chan) {
      p->state = RUNNABLE;
    }
    release(&p->lock);
  }
}

void sleep(void *chan, struct spinlock *lk) {
  proc_t *p = current_proc();

  acquire(&p->lock);
  release(lk);

  p->chan = chan;
  p->state = SLEEPING;

  sched();

  p->chan = NULL;

  release(&p->lock);
  acquire(lk);
}


