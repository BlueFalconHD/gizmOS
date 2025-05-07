#include "proc.h"
#include "lib/cpu.h"
#include "lib/print.h"
#include "lib/result.h"
#include "lib/spinlock.h"
#include "lib/str.h"
#include "limine_requests.h"
#include "platform/interrupts.h"
#include "platform/registers.h"

#include <lib/memory.h>
#include <lib/panic.h>
#include <lib/types.h>
#include <mem_layout.h>
#include <page_table.h>
#include <physical_alloc.h>
#include <stdbool.h>

// #define DBG

proc_t proc[NPROC];

uint64_t pid = 0;
struct spinlock pid_lock;

extern char trampoline[];
extern char uservec[];
extern char userret[];

extern void trap_vector();

extern void swtch(struct context *, struct context *);

g_bool setup_process_kernel_stack(proc_t *p, uint8_t pidx) {
  if (!p)
    return false;

  if (!p->kstack) {

    void *kstack = alloc_page();

    uint64_t kstackpaddr = V2P((uint64_t)kstack);
    uint64_t kstackvaddr = KSTACK(pidx);

#ifdef DBG

    printf("%{type: int}: %{type: hex} -> %{type: hex}\n", PRINT_FLAG_BOTH,
           pidx, kstackvaddr, kstackpaddr);

#endif

    if (!map_page(shared_page_table, kstackvaddr, kstackpaddr,
                  PTE_R | PTE_W | PTE_X | PTE_V)) {
      panic_msg("Kernel stack mapping failed");
      printf("pidx: %{type: int}", PRINT_FLAG_BOTH, pidx);
      panic_loc("setup_process_kernel_stack");
    }

    p->kstack = kstackvaddr;

    // // set first byte of kstack to 0xAA
    // *(uint8_t *)kstack = 0xAA;

    // // read from kstackvaddr to verify
    // uint8_t *kstack_check = (uint8_t *)kstackvaddr;
    // if (*kstack_check != 0xAA) {
    //   panic_msg("Kernel stack verification failed");
    //   printf("pidx: %{type: int}", PRINT_FLAG_BOTH, pidx);
    //   panic_loc("setup_process_kernel_stack");
    // }

    // // keep reading downwards from kstackvaddr to check if the gaurd page
    // is
    // // working
    // for (int i = 1; i < 0xFFFF; i++) {
    //   volatile uint8_t *addr = (uint8_t *)(kstackvaddr - i);
    //   volatile uint8_t val = *addr; // <-- Add this line to force the read
    //   (void)val;                    // Prevent unused variable warning if
    //   needed
    // }
  }

  return true;
}

// forward declaration
void usertrap(void);

void user_trap_ret(void) {
  proc_t *p = current_proc();

  PS_disable_interrupts();

  // printf("user_trap_ret: p->trapframe->epc = %{type: hex}\n",
  // PRINT_FLAG_BOTH,
  //        (uint64_t)p->trapframe->epc);

  uint64_t trampoline_uservec = TRAMPOLINE + (uservec - trampoline);
  PS_set_trap_vector(trampoline_uservec);

  // printf("user_trap_ret: trampoline_uservec = %{type: hex}\n",
  // PRINT_FLAG_BOTH,
  //        trampoline_uservec);

  p->trapframe->kernel_satp = PS_get_atp();
  p->trapframe->kernel_sp = p->kstack + PAGE_SIZE;
  p->trapframe->kernel_trap = (uint64_t)usertrap;
  p->trapframe->kernel_hartid = P_get_thread_ptr();

  // printf("kernel_trap = %{type: hex}\n", PRINT_FLAG_BOTH,
  //        p->trapframe->kernel_trap);

  uint64_t x = PS_get_status();
  x &= ~SSTATUS_SPP;
  x |= SSTATUS_SPIE;
  PS_set_status(x);

  PS_set_exception_pc(p->trapframe->epc);

  uint64_t table_pa = ((uint64_t)p->pagetable) - hhdm_offset;
  uint64_t table_ppn = table_pa >> 12;
  uint64_t satp_value = (uint64_t)8ULL << 60; // MODE field for SV39
  satp_value |= table_ppn;

  uint64_t trampoline_userret = TRAMPOLINE + (userret - trampoline);

  // printf("In user_trap_ret: about to call trampoline_userret (at %{type:
  // hex}) "
  //        "with satp_value = %{type: hex}\n",
  //        PRINT_FLAG_BOTH, trampoline_userret, satp_value);

  // printf("Is trampoline_userret mapped? %{type: int}\n", PRINT_FLAG_BOTH,
  //        is_addr_mapped(shared_page_table, trampoline_userret));

  ((void (*)(uint64_t))trampoline_userret)(satp_value);
}

void forkret() {
  release(&current_proc()->lock);
  user_trap_ret();
}

g_bool initialize_processes() {
  for (uint8_t i = 0; i < NPROC; i++) {
    proc_t *p = &proc[i];
    initlock(&p->lock, "proc");
    p->state = UNUSED;
    setup_process_kernel_stack(p, i);
  }

  // lock init
  initlock(&pid_lock, "pid_lock");
}

uint64_t allocate_pid() {
  uint64_t new_pid = 0;

  acquire(&pid_lock);
  new_pid = pid;
  pid++;
  release(&pid_lock);

  return new_pid;
}

page_table_t *allocate_process_page_table(proc_t *p) {
  page_table_t *pt = alloc_page();
  if (!pt) {
    return NULL;
  }

  // zero out the page table
  memset(pt, 0, PAGE_SIZE);

  if (!map_page(pt, TRAMPOLINE, V2P((uint64_t)trampoline),
                PTE_R | PTE_W | PTE_X | PTE_V)) {
    free_page(pt);
    return NULL;
  }

  if (!map_page(pt, TRAPFRAME, V2P((uint64_t)p->trapframe),
                PTE_R | PTE_W | PTE_X | PTE_V)) {
    free_page(pt);
    return NULL;
  }

  return pt;
}

RESULT_TYPE(proc_t *) allocate_process() {
  proc_t *p = NULL;

  for (uint8_t i = 0; i < NPROC; i++) {
    p = &proc[i];
    acquire(&p->lock);
    if (p->state == UNUSED) {
      goto found;
    } else {
      release(&p->lock);
    }
  }

  return RESULT_FAILURE(RESULT_BUSY);

found:

  p->pid = allocate_pid();
  p->state = USED;

  // trapframe
  struct trapframe *tf = alloc_page();
  if (!tf) {
    // TODO: free proc
    release(&p->lock);
    return RESULT_FAILURE(RESULT_NOMEM);
  }

  p->trapframe = tf;

  // page table
  page_table_t *pt = allocate_process_page_table(p);
  if (!pt) {
    free_page(p->trapframe);
    p->trapframe = NULL;
    release(&p->lock);
    return RESULT_FAILURE(RESULT_NOMEM);
  }

  p->pagetable = pt;

  // zero out context
  memset(&p->context, 0, sizeof(struct context));

  // TODO: forkret
  p->context.ra = (uint64_t)forkret;
  p->context.sp = p->kstack + PAGE_SIZE;

  printf("alloc proc kstack = %{type: hex}\n", PRINT_FLAG_BOTH, p->kstack);

  return RESULT_SUCCESS(p);
}

void free_process(proc_t *p) {
  if (!p)
    return;

  if (p->pagetable) {
    free_page(p->pagetable);
    p->pagetable = NULL;
  }

  if (p->trapframe) {
    free_page(p->trapframe);
    p->trapframe = NULL;
  }

  p->sz = 0;
  p->pid = 0;
  p->parent = 0;
  p->name[0] = 0;
  p->chan = 0;
  p->killed = 0;
  p->xstate = 0;

  p->state = UNUSED;
}

void scheduler() {
  proc_t *p = NULL;
  cpu_t *c = current_cpu();

  c->proc = 0;

  for (;;) {
    PS_enable_interrupts();

    g_bool found = false;

    for (uint8_t i = 0; i < NPROC; i++) {
      p = &proc[i];
      acquire(&p->lock);
      if (p->state == RUNNABLE) {
        p->state = RUNNING;
        c->proc = p;

        printf("Switching to process %{type: str} (pid %{type: int})\n",
               PRINT_FLAG_BOTH, p->name, p->pid);
        printf("Will jump to %{type: hex}\n", PRINT_FLAG_BOTH,
               (uint64_t)p->context.ra);

        swtch(&c->context, &p->context);

        c->proc = 0;
        found = true;
      }
      release(&p->lock);
    }

    if (!found) {
      PS_enable_interrupts();
      asm volatile("wfi");
    }
  }
}

void sched(void) {
  int intena;
  proc_t *p = current_proc();
  cpu_t *c = current_cpu();

  if (!holding(&p->lock))
    panic("sched p->lock");
  if (c->noff != 1)
    panic("sched locks");
  if (p->state == RUNNING)
    panic("sched running");
  if (PS_get_interrupt_enabled())
    panic("sched interruptible");

  intena = c->intena;
  swtch(&p->context, &c->context);
  c->intena = intena;
}

void yield(void) {
  proc_t *p = current_proc();
  acquire(&p->lock);
  p->state = RUNNABLE;
  sched();
  release(&p->lock);
}

uint8_t initcode[] = {0x17, 0x05, 0x00, 0x00, 0x13, 0x05, 0x45, 0x02, 0x97,
                      0x05, 0x00, 0x00, 0x93, 0x85, 0x35, 0x02, 0x93, 0x08,
                      0x70, 0x00, 0x73, 0x00, 0x00, 0x00, 0x93, 0x08, 0x20,
                      0x00, 0x73, 0x00, 0x00, 0x00, 0xef, 0xf0, 0x9f, 0xff,
                      0x2f, 0x69, 0x6e, 0x69, 0x74, 0x00, 0x00, 0x24, 0x00,
                      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

void first_process() {
  proc_t *p;

  result_t rp = allocate_process();
  if (!result_is_ok(rp)) {
    panic("Failed to allocate first process");
  }

  p = (proc_t *)result_unwrap(rp);

  void *initcode_page = alloc_page();

  if (!map_page(p->pagetable, 0, V2P((uint64_t)initcode_page),
                PTE_V | PTE_R | PTE_W | PTE_X | PTE_U)) {
    panic("Failed to map first process");
  }

  memset(initcode_page, 0, PAGE_SIZE);
  memcpy(initcode_page, initcode, sizeof(initcode));

  p->sz = PAGE_SIZE;

  p->trapframe->epc = 0;
  p->trapframe->sp = PAGE_SIZE;

  strncopy(p->name, "init", sizeof(p->name));

  p->state = RUNNABLE;

  release(&p->lock);
}

void usertrap(void) {
  int which_dev = 0;

  if ((PS_get_status() & SSTATUS_SPP) != 0)
    panic("usertrap: not from user mode");

  PS_set_trap_vector((uint64_t)trap_vector);

  proc_t *p = current_proc();

  // save user program counter.
  p->trapframe->epc = PS_get_exception_pc();

  if (PS_get_exception_cause() == 8) {
    // system call
    // if (killed(p))
    //   exit(-1);

    // sepc points to the ecall instruction,
    // but we want to return to the next instruction.
    p->trapframe->epc += 4;

    // an interrupt will change sepc, scause, and sstatus,
    // so enable only now that we're done with those registers.
    PS_enable_interrupts();

    // TODO: actual syscall handling

    printf("syscall (from pid=%{type: int}), # = %{type: int}\n",
           PRINT_FLAG_BOTH, p->pid, p->trapframe->a7);

    // default ignore
    p->trapframe->a0 = -1;
  }
  // } else if((which_dev = devintr()) != 0){
  //   // ok
  // } else {
  //   printf("usertrap(): unexpected scause 0x%lx pid=%d\n", r_scause(),
  //   p->pid); printf("            sepc=0x%lx stval=0x%lx\n", r_sepc(),
  //   r_stval()); setkilled(p);
  // }

  // if(killed(p))
  //   exit(-1);

  // give up the CPU if this is a timer interrupt.
  if (which_dev == 2)
    yield();

  user_trap_ret();
}
