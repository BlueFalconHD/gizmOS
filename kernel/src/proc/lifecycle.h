#pragma once

#include "process.h"

RESULT_TYPE(proc_t *) make_proc();
void free_process(proc_t *p);
void reparent(proc_t *p);
void exit(uint64_t status);
uint64_t wait(uint64_t address);
RESULT_TYPE(void) kill(uint64_t pid);
void setkilled(proc_t *p);
g_bool killed(proc_t *p);
uint64_t fork(void);

// Create a user process from a raw code buffer mapped at VA=0
RESULT_TYPE(proc_t *) proc_from_code(uint8_t code[], uint64_t size,
                                     const char *name);

// Create a user process by loading and mapping a Vessel at a disk root path (8.3)
RESULT_TYPE(proc_t *) proc_from_vessel_path(const char *path83, const char *name);

// Create a user process and install argv on its initial user stack.
// 'name' becomes argv[0] (if NULL, path83 is used). 'argv' supplies argv[1..argc].
RESULT_TYPE(proc_t *) proc_from_vessel_path_args(const char *path83,
                                                 const char *name,
                                                 uint64_t argc,
                                                 const char * const *argv);

