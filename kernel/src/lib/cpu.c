#include "cpu.h"
#include <lib/panic.h>
#include <platform/registers.h>

struct cpu cpus[NCPU];

struct cpu *current_cpu(void) {
  uint64_t tp = P_get_thread_ptr();
  return &cpus[tp];
};

proc_t *current_proc(void) {
  struct cpu *c = current_cpu();
  return c->proc;
}

void intr_push_off() {
  int old = PS_get_interrupt_enabled();

  PS_disable_interrupts();
  if (current_cpu()->intr_disable_depth == 0)
    current_cpu()->prev_interrupts_enabled = old;
  current_cpu()->intr_disable_depth += 1;
}

void intr_pop_off() {
  struct cpu *c = current_cpu();
  if (PS_get_interrupt_enabled())
    panic("interrupts are enabled during pop_off");
  if (c->intr_disable_depth < 1)
    panic("noff underflow, should NOT happen");
  c->intr_disable_depth -= 1;
  if (c->intr_disable_depth == 0 && c->prev_interrupts_enabled)
    PS_enable_interrupts();
}
