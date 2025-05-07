#pragma once

#include "lib/panic.h"
#include "platform/interrupts.h"
#include <lib/macros.h>
#include <lib/types.h>

#define NCPU 8

struct context {
  uint64_t ra;
  uint64_t sp;

  // callee-saved
  uint64_t s0;
  uint64_t s1;
  uint64_t s2;
  uint64_t s3;
  uint64_t s4;
  uint64_t s5;
  uint64_t s6;
  uint64_t s7;
  uint64_t s8;
  uint64_t s9;
  uint64_t s10;
  uint64_t s11;
};

// Per-CPU state.
struct cpu {
  struct proc *proc;          // The process running on this cpu, or null.
  struct context context;     // swtch() here to enter scheduler().
  int noff;                   // Depth of push_off() nesting.
  int intena;                 // Were interrupts enabled before push_off()?
};

extern struct cpu cpus[NCPU];

typedef struct cpu cpu_t;

struct cpu *current_cpu(void);
struct proc *current_proc(void);

void intr_push_off();
void intr_pop_off();
