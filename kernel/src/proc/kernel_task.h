#pragma once

#include "process.h"

void kernel_task_wrapper(void);
RESULT_TYPE(proc_t *) make_kernel_task(void (*entry)(void *), void *arg,
                                       const char *name);


