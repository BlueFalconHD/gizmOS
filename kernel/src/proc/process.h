#include <lib/context.h>
#include <lib/mailbox.h>
#include <lib/notification.h>
#include <lib/spinlock.h>
#include <page_table.h>

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

typedef enum { UNUSED, USED, SLEEPING, RUNNABLE, RUNNING, ZOMBIE } procstate;

#define PROC_PRIORITY_HIGH 0
#define PROC_PRIORITY_NORMAL 10
#define PROC_PRIORITY_LOW 20
#define PROC_PRIORITY_FLUSH 30

typedef struct proc {
  struct spinlock lock;
  procstate state;
  uint8_t priority;
  void *chan;
  int killed;
  int xstate;
  int pid;

  struct proc *parent;

  uint64_t kstack;
  context_t context;

  uint64_t sz;
  page_table_t *pagetable;
  struct trapframe *trapframe;

  char name[16];

  g_bool is_kernel;
  mailbox_t *mailbox;
} proc_t;
