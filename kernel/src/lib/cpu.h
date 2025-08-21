#pragma once

#include "proc.h"
#include <lib/context.h>
#include <lib/macros.h>
#include <lib/types.h>
#include <platform/interrupts.h>

#define NCPU 8

/**
 * Per-CPU state
 */
typedef struct cpu {
  proc_t *proc;                // The process running on this cpu, or null.
  context_t context;           // swtch() here to enter scheduler.
  int intr_disable_depth;      // Depth of push_off nesting.
  int prev_interrupts_enabled; // Were interrupts enabled before push_off?
} cpu_t;

extern cpu_t cpus[NCPU];

cpu_t *current_cpu(void);
proc_t *current_proc(void);

/**
 * Push off interrupts for the current CPU. Disables interrupts and remembers
 * previous state.
 */
void intr_push_off();

/**
 * Pop off interrupts for the current CPU. Restores interrupts to the previous
 * state once the depth reaches zero.
 */
void intr_pop_off();
