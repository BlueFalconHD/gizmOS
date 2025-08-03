#pragma once

#include "lib/mailbox.h"
#include "lib/spinlock.h"
#include <lib/context.h>
#include <lib/result.h>
#include <page_table.h>

#define NPROC 64 /* maximum number of processes */

struct trapframe {
  uint64_t kernel_satp;   /* kernel page table (satp value)      */
  uint64_t kernel_sp;     /* top of kernel stack for this proc   */
  uint64_t kernel_trap;   /* usertrap()                          */
  uint64_t epc;           /* saved user pc                       */
  uint64_t kernel_hartid; /* saved kernel tp (=hartid)           */

  /* callee-saved & caller-saved registers */
  uint64_t ra;
  uint64_t sp;
  uint64_t gp;
  uint64_t tp;
  uint64_t t0;
  uint64_t t1;
  uint64_t t2;
  uint64_t s0;
  uint64_t s1;
  uint64_t a0;
  uint64_t a1;
  uint64_t a2;
  uint64_t a3;
  uint64_t a4;
  uint64_t a5;
  uint64_t a6;
  uint64_t a7;
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
  uint64_t t3;
  uint64_t t4;
  uint64_t t5;
  uint64_t t6;
};

enum procstate { UNUSED, USED, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };

/* Process priorities - lower numbers = higher priority */
#define PROC_PRIORITY_HIGH 0    /* High priority kernel tasks */
#define PROC_PRIORITY_NORMAL 10 /* Normal processes */
#define PROC_PRIORITY_LOW 20    /* Low priority background tasks */
#define PROC_PRIORITY_FLUSH 30  /* Framebuffer flush daemon - runs last */

struct proc {
  /* locks & scheduling */
  struct spinlock lock;
  enum procstate state;
  uint8_t priority; /* Process priority (lower = higher priority) */
  void *chan;
  int killed;
  int xstate;
  int pid;

  struct proc *parent;

  /* kernel context / stack */
  uint64_t kstack;
  context_t context;

  uint64_t sz;
  page_table_t *pagetable;
  struct trapframe *trapframe; /* TRAPFRAME page */

  char name[16];

  g_bool is_kernel; /* true if this is a kernel task */

  mailbox_t *mailbox; /* mailbox for notifications */
};

typedef struct proc proc_t;
extern proc_t proc[NPROC];

g_bool initialize_processes();
void first_process();
RESULT_TYPE(proc_t *) make_proc();
void scheduler();
void yield(void);
g_bool proc_grow(proc_t *p, uint64_t n);
g_bool proc_shrink(proc_t *p, uint64_t n);
RESULT_TYPE(void) proc_resize(int n);
RESULT_TYPE(proc_t *)
proc_from_code(uint8_t code[], uint64_t size, const char *name);

RESULT_TYPE(proc_t *)
make_kernel_task(void (*entry)(void *), void *arg, const char *name);
