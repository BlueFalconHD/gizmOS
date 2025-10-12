#pragma once

#include "process.h"
#include <lib/spinlock.h>

#define NPROC 64

/* Process table and related globals */
extern proc_t processes[NPROC];
extern struct spinlock wait_lock;
extern proc_t *init_proc;

/* Lifecycle and table helpers */
g_bool initialize_processes();
uint64_t allocate_pid();
g_bool setup_process_kernel_stack(proc_t *p, uint8_t index);
