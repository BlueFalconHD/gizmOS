#pragma once

#include "lib/panic.h"
#include "platform/interrupts.h"
#include <lib/context.h>
#include "proc.h"
#include <lib/macros.h>
#include <lib/types.h>

#define NCPU 8

// Per-CPU state.
struct cpu {
  proc_t *proc;          // The process running on this cpu, or null.
  context_t context;     // swtch() here to enter scheduler().
  int noff;                   // Depth of push_off() nesting.
  int intena;                 // Were interrupts enabled before push_off()?
};

extern struct cpu cpus[NCPU];

typedef struct cpu cpu_t;

struct cpu *current_cpu(void);
proc_t *current_proc(void);

void intr_push_off();
void intr_pop_off();
