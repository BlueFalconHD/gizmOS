#include "cpu.h"
#include "platform/registers.h"

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
  if (current_cpu()->noff == 0)
    current_cpu()->intena = old;
  current_cpu()->noff += 1;
}

void intr_pop_off() {
  struct cpu *c = current_cpu();
  if (PS_get_interrupt_enabled())
    panic("pop_off - interruptible");
  if (c->noff < 1)
    panic("pop_off");
  c->noff -= 1;
  if (c->noff == 0 && c->intena)
    PS_enable_interrupts();
}
