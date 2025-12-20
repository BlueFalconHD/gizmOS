#pragma once

#include "process.h"
#include <lib/result.h>

// Create a schedulable thread that shares the given leader's address space.
RESULT_TYPE(proc_t *) make_thread(proc_t *leader);

// Thread syscalls
uint64_t thread_create(uint64_t entry_va, uint64_t arg, uint64_t stack_top);
uint64_t thread_join(uint64_t tid, uint64_t status_out_uva);
void     thread_exit(uint64_t status);

// Called when reaping a process leader to release any remaining thread slots.
void thread_group_reap(proc_t *leader);

