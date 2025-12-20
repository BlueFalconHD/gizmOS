#include "scheduler.h"
#include "process.h"
#include "process_table.h"
#include <lib/cpu.h>
#include <lib/panic.h>
#include <platform/interrupts.h>
#include <platform/registers.h>

extern void swtch(context_t *, context_t *);

void scheduler() {
  proc_t *p = NULL;
  cpu_t *c = current_cpu();
  static uint64_t schedule_count = 0; (void)schedule_count;
  static uint8_t rr_index = 0;
  const int cpu_id = (int)P_get_thread_ptr();

  c->proc = 0;

  for (;;) {
    PS_enable_interrupts();

    uint8_t runnable_count = 0;
    uint8_t min_priority = 255;
    int selected_index = -1;
    proc_t *selected_proc = NULL;

    for (uint8_t offset = 0; offset < NPROC; offset++) {
      uint8_t i = (rr_index + offset) % NPROC;
      p = &processes[i];
      acquire(&p->lock);
      if (p->state == RUNNABLE) {
        proc_t *leader = proc_group(p);
        int running = leader ? __atomic_load_n(&leader->tg_running_cpu, __ATOMIC_ACQUIRE) : -1;
        if (running == -1 || running == cpu_id) {
          runnable_count++;
          if (p->priority < min_priority) {
            min_priority = p->priority;
          }
        }
      }
      release(&p->lock);
    }

    if (runnable_count > 0) {
      for (uint8_t offset = 0; offset < NPROC; offset++) {
        uint8_t i = (rr_index + offset) % NPROC;
        p = &processes[i];
        acquire(&p->lock);
        if (p->state == RUNNABLE && p->priority == min_priority) {
          proc_t *leader = proc_group(p);
          if (leader) {
            int expected = -1;
            if (__atomic_compare_exchange_n(&leader->tg_running_cpu, &expected, cpu_id,
                                            false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE) ||
                expected == cpu_id) {
              selected_proc = p;
              selected_index = i;
              break;
            }
          }
        }
        release(&p->lock);
      }
    }

    if (selected_proc) {
      selected_proc->state = RUNNING;
      c->proc = selected_proc;

      swtch(&c->context, &selected_proc->context);

      c->proc = 0;

      proc_t *leader = proc_group(selected_proc);
      if (leader) {
        int expected = cpu_id;
        (void)__atomic_compare_exchange_n(&leader->tg_running_cpu, &expected, -1,
                                          false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE);
      }
      release(&selected_proc->lock);
      schedule_count++;

      rr_index = (uint8_t)((selected_index + 1) % NPROC);
    } else {
      PS_enable_interrupts();
      asm volatile("wfi");
    }
  }
}

void sched(void) {
  int intena;
  proc_t *p = current_proc();
  cpu_t *c = current_cpu();

  if (!holding(&p->lock))
    panic("sched p->lock");
  if (c->intr_disable_depth != 1)
    panic("sched locks");
  if (p->state == RUNNING)
    panic("sched running");
  if (PS_get_interrupt_enabled())
    panic("sched interruptible");

  intena = c->prev_interrupts_enabled;
  swtch(&p->context, &c->context);
  c->prev_interrupts_enabled = intena;
}

void yield(void) {
  proc_t *p = current_proc();
  acquire(&p->lock);
  p->state = RUNNABLE;
  sched();
  release(&p->lock);
}

