#pragma once

#include <lib/context.h>
#include <lib/spinlock.h>
#include <page_table.h>
#include <lib/types.h>
#include <lib/result.h>
#include "notification_types.h"
#include "spine.h"
#include "descriptor.h"

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

  // Thread groups:
  // - A "process" is the thread-group leader (tg_leader == this, is_thread==0).
  // - Additional threads share the leader's address space (pagetable) and are
  //   scheduled as independent proc_t instances (is_thread==1).
  // - Use proc_group(p) when accessing process-wide resources.
  struct proc *tg_leader;      // NULL until initialized; leader points to self
  uint32_t     tgid;           // thread group id (leader pid)
  uint8_t      is_thread;      // 0 = leader (process), 1 = thread
  int32_t      tg_running_cpu; // -1 if not running; used to prevent SMP overlap

  struct proc *parent;

  uint64_t kstack;
  context_t context;

  uint64_t sz;
  page_table_t *pagetable;
  struct trapframe *trapframe;
  uint64_t heap_base;
  uint64_t stack_base;
  uint64_t stack_top;

  char name[16];

  /* Spine IPC */
  uint64_t        spine_token;
  char            spine_service[16];
  struct spinlock spine_lock;
  spine_msg_t     spine_queue[SPINE_MSG_QUEUE_SIZE];
  uint32_t        spine_q_head;
  uint32_t        spine_q_tail;
  uint32_t        spine_pending;
  uint64_t        spine_stats_dropped;
  uint64_t        spine_wait_deadline; // absolute spine tick, 0 if none
  uint8_t         spine_wait_chan;     // unique sleep channel address

  g_bool is_kernel;
  /* Notifications */
  notif_msg_t     notif_queue[NOTIF_QUEUE_SIZE];
  uint32_t        notif_q_head;
  uint32_t        notif_q_tail;
  uint64_t        notif_seq;
  uint8_t         notif_pending;
  uint64_t        notif_stats_dropped;
  notif_handler_t notif_handlers[NOTIF_MAX_TYPE];
  notif_ctx_t     notif_ctx;
  uint64_t        notif_userbuf_base; // user VA for buffer+stub
  uint64_t        notif_userbuf_size;
  notif_ctx_stack_t notif_stack; /* nested delivery contexts */

  /* Unified descriptors */
  descriptor_t *desc_table[PROC_MAX_DESC];

  /* File descriptors (legacy path/device) */
  #define PROC_MAX_FD 32
  struct fs_file *fd_table[PROC_MAX_FD];

  /* Object handles (compatibility) */
  #define PROC_MAX_OBJH 128
  uint64_t objh_ids[PROC_MAX_OBJH];   /* UINT64_MAX means free slot */
  uint32_t objh_flags[PROC_MAX_OBJH]; /* open flags */
} proc_t;

static inline proc_t *proc_group(proc_t *p) {
  if (!p) return NULL;
  return p->tg_leader ? p->tg_leader : p;
}

static inline g_bool proc_is_thread(proc_t *p) {
  if (!p) return false;
  proc_t *g = proc_group(p);
  return (g != NULL && g != p);
}
