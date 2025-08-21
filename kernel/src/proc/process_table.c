#include "process_table.h"
#include "process.h"
#include <lib/spinlock.h>

proc_t processes[NPROC];

uint64_t current_pid = 0;
struct spinlock current_pid_lock;

struct spinlock wait_lock;

proc_t *init_proc;
