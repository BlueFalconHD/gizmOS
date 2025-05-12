#include "proc.h"
#include "lib/ansi.h"
#include "lib/context.h"
#include "lib/cpu.h"
#include "lib/print.h"
#include "lib/result.h"
#include "lib/spinlock.h"
#include "lib/str.h"
#include "lib/usermem.h"
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

struct spinlock wait_lock;

extern char trampoline[];
extern char uservec[];
extern char userret[];

extern void trap_vector();

extern void swtch(context_t *, context_t *);

proc_t *init_proc;

#define PGROUNDUP(sz) (((sz) + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1))
#define PGROUNDDOWN(sz) ((sz) & ~(PAGE_SIZE - 1))

g_bool uvmdealloc(proc_t *p, uint64_t oldsz, uint64_t newsz); /* fwd */

/* grow from oldsz up to newsz (page-aligned) */
g_bool uvmalloc(proc_t *p, uint64_t oldsz, uint64_t newsz) {
  if (newsz < oldsz)
    return true;

  oldsz = PGROUNDUP(oldsz);
  for (uint64_t a = oldsz; a < newsz; a += PAGE_SIZE) {
    void *mem = alloc_page();
    if (!mem)
      return false;
    memset(mem, 0, PAGE_SIZE);
    if (!map_page(p->pagetable, a, V2P((uint64_t)mem),
                  PTE_R | PTE_W | PTE_X | PTE_U | PTE_V)) {
      free_page(mem);
      uvmdealloc(p, a, oldsz); /* roll back */
      return false;
    }
  }
  p->sz = newsz;
  return true;
}

/* shrink from oldsz down to newsz, freeing pages */
g_bool uvmdealloc(proc_t *p, uint64_t oldsz, uint64_t newsz) {
  if (newsz >= oldsz)
    return true;

  for (uint64_t a = PGROUNDUP(newsz); a < oldsz; a += PAGE_SIZE) {
    uint64_t pa = 0;
    if (!get_physical_address(p->pagetable, a, &pa))
      continue;
    if (!unmap_page(p->pagetable, a))
      return false;
    free_page((void *)(pa + hhdm_offset));
  }
  p->sz = newsz;
  return true;
}

/* clone user memory from src to dst up to sz bytes */
g_bool uvmcopy(page_table_t *src, page_table_t *dst, uint64_t sz) {
  for (uint64_t a = 0; a < sz; a += PAGE_SIZE) {
    uint64_t pa = 0;
    if (!get_physical_address(src, a, &pa))
      return false;

    void *mem = alloc_page();
    if (!mem)
      return false;
    memcpy(mem, (void *)(pa + hhdm_offset), PAGE_SIZE);

    if (!map_page(dst, a, V2P((uint64_t)mem),
                  PTE_R | PTE_W | PTE_X | PTE_U | PTE_V)) {
      free_page(mem);
      return false;
    }
  }
  return true;
}

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

  // printf("user_trap_ret: p->pid = %{type: int}\n", PRINT_FLAG_BOTH, p->pid);
  // printf("user_trap_ret: p->name = %{type: str}\n", PRINT_FLAG_BOTH,
  // p->name);

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

  // printf("In user_trap_ret: about to call trampoline_userret (at %{type:hex})
  // "
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

RESULT_TYPE(proc_t *) make_proc() {
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
  memset(&p->context, 0, sizeof(context_t));

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

        // printf("Switching to process %{type: str} (pid %{type: int})\n",
        // PRINT_FLAG_BOTH, p->name, p->pid);
        // printf("Will jump to %{type: hex}\n", PRINT_FLAG_BOTH,
        // (uint64_t)p->context.ra);

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

uint8_t initcode[] = {
    // ... (first 24 bytes are unchanged) ...
    0x17, 0x05, 0x00, 0x00, // 0x00: auipc a0, 0
    0x13, 0x05, 0x45, 0x02, // 0x04: addi  a0, a0, 36
    0x97, 0x05, 0x00, 0x00, // 0x08: auipc a1, 0
    0x93, 0x85, 0x35, 0x02, // 0x0c: addi  a1, a1, 35
    0x93, 0x08, 0x70, 0x00, // 0x10: li    a7, 7
    0x73, 0x00, 0x00, 0x00, // 0x14: ecall

    // Modified part starts here (index 24, address 0x18)
    0x73, 0x00, 0x50, 0x10, // 0x18: wfi              (was li a7, 2)
    0x6F, 0xF0, 0xFF, 0xFF, // 0x1c: j 0x18           (was ecall)
    0x13, 0x00, 0x00, 0x00, // 0x20: nop              (was jal ra, -1028)

    // Data section (unchanged)
    0x2f, 0x69, 0x6e, 0x69, // 0x24: "/ini"
    0x74, 0x00, 0x00, 0x24, // 0x28: "t\0\0$"
    0x00, 0x00, 0x00, 0x00, // 0x2c: padding
                            // End of 48-byte array
};

void first_process() {
  proc_t *p;

  result_t rp = make_proc();
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

  init_proc = p;

  release(&p->lock);
}

RESULT_TYPE(proc_t *)
proc_from_code(uint8_t code[], uint64_t size, const char *name) {
  proc_t *p = NULL;

  result_t rp = make_proc();
  if (!result_is_ok(rp)) {
    return RESULT_FAILURE(RESULT_NOMEM);
  }

  p = (proc_t *)result_unwrap(rp);

  /* Allocate user memory, copy code, and set up process state */
  uint64_t newsz = PGROUNDUP(size);
  for (uint64_t a = 0; a < newsz; a += PAGE_SIZE) {
    /* allocate a physical page */
    void *mem = alloc_page();
    if (!mem) {
      /* roll-back any pages we already mapped */
      uvmdealloc(p, a, 0);
      free_process(p);
      release(&p->lock);
      return RESULT_FAILURE(RESULT_NOMEM);
    }

    /* zero page before use */
    memset(mem, 0, PAGE_SIZE);

    /* map into the new process's page table */
    if (!map_page(p->pagetable, a, V2P((uint64_t)mem),
                  PTE_V | PTE_R | PTE_W | PTE_X | PTE_U)) {
      free_page(mem);
      uvmdealloc(p, a, 0);
      free_process(p);
      release(&p->lock);
      return RESULT_FAILURE(RESULT_ERROR);
    }

    /* copy the corresponding slice of code[] into this page */
    uint64_t copy_sz = (size - a < PAGE_SIZE) ? (size - a) : PAGE_SIZE;
    if (copy_sz > 0)
      memcpy(mem, code + a, copy_sz);
  }

  /* record the size of the process's memory image */
  p->sz = newsz;

  /* initialise trapframe for entry to user mode */
  p->trapframe->epc = 0;    /* first instruction */
  p->trapframe->sp = newsz; /* stack pointer just above program */

  /* give the process a name, if provided */
  if (name != NULL)
    strncopy(p->name, name, sizeof(p->name));

  /* mark runnable and release the lock so scheduler can pick it up */
  p->state = RUNNABLE;
  release(&p->lock);

  return RESULT_SUCCESS(p);
}

void wakeup(void *chan) {
  for (uint8_t i = 0; i < NPROC; i++) {
    proc_t *p = &proc[i];
    acquire(&p->lock);
    if (p->state == SLEEPING && p->chan == chan) {
      p->state = RUNNABLE;
    }
    release(&p->lock);
  }
}

void reparent(proc_t *p) {
  proc_t *pp = current_proc();

  for (uint8_t i = 0; i < NPROC; i++) {
    proc_t *child = &proc[i];
    if (child->parent == p) {
      child->parent = init_proc;
      wakeup(init_proc);
    }
  }
}

void exit(uint64_t status) {
  proc_t *p = current_proc();

  if (p == init_proc)
    panic("init proc exiting");

  acquire(&wait_lock);

  reparent(p);
  wakeup(p->parent);

  acquire(&p->lock);
  p->xstate = status;
  p->state = ZOMBIE;

  release(&wait_lock);

  sched();
  panic("zombie exit");
}

void usertrap(void) {
  int which_dev = 0;

  if ((PS_get_status() & SSTATUS_SPP) != 0)
    panic("usertrap: not from user mode");

  PS_set_trap_vector((uint64_t)trap_vector);

  proc_t *p = current_proc();

  // printf("p->pid = %{type: int}\n", PRINT_FLAG_BOTH, p->pid);
  // printf("usertrap: p->name = %{type: str}\n", PRINT_FLAG_BOTH, p->name);

  // uint64_t pa_code_start = 0;
  // if (get_physical_address(p->pagetable, 0, &pa_code_start)) {
  //   uint64_t kernel_va_code_start = pa_code_start + hhdm_offset;
  //   printf("usertrap: kernel_va_for_proc_code_start = %{type: hex}\n",
  //          PRINT_FLAG_BOTH, kernel_va_code_start);
  // } else {
  //   printf("usertrap: failed to get physical address for proc_code_start\n",
  //          PRINT_FLAG_BOTH);
  // }

  // printf("usertrap: p->sz = %{type: hex}\n", PRINT_FLAG_BOTH, p->sz);

  // // parse scause to determine if it is an interrupt or exception
  // if (PS_get_exception_cause() & 0x8000000000000000) {
  //   // interrupt
  //   printf(ANSI_APPLY(ANSI_COLOR_GREEN, "scause %{type: hex}\n"),
  //          PRINT_FLAG_BOTH, PS_get_exception_cause() & 0x7FFFFFFFFFFFFFFF);
  // } else {
  //   printf(ANSI_APPLY(ANSI_COLOR_RED, "scause %{type: hex}\n"),
  //   PRINT_FLAG_BOTH,
  //          PS_get_exception_cause());
  // }

  if (PS_get_exception_cause() == 0x2) {
    // print out faulting address and the data there
    uint64_t faulting_address = PS_get_exception_pc();
    uint64_t fault_pa = 0;

    if (get_physical_address(p->pagetable, faulting_address, &fault_pa)) {
      uint64_t fault_va = fault_pa + hhdm_offset;
      printf("Faulting address: %{type: hex}\n", PRINT_FLAG_BOTH, fault_va);
      printf("Data at faulting address: 0x%{type: hex}\n", PRINT_FLAG_BOTH,
             *(uint64_t *)fault_va);
    } else {
      printf("Failed to get physical address for faulting address\n",
             PRINT_FLAG_BOTH);
    }
  }

  // save user program counter.
  p->trapframe->epc = PS_get_exception_pc();

  // scause == 8 means a system call (ecall from user mode)
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

    int callnum = p->trapframe->a7;

    // printf("syscall (from pid=%{type: int}), # = %{type: int}\n",
    // PRINT_FLAG_BOTH, p->pid, p->trapframe->a7);

    if (callnum == 2) {
      // exit
      // int exitcode = p->trapframe->a0;
      // exit(exitcode);
    } else if (callnum == 7) {
      // print("proc a\n", PRINT_FLAG_BOTH);
    } else if (callnum == 8) {
      // print("proc b\n", PRINT_FLAG_BOTH);
    }

    // default ignore
    // p->trapframe->a0 = -1;
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

  // // give up the CPU if this is a timer interrupt.
  if (PS_get_exception_cause() == 0x8000000000000005) {
    // print(ANSI_APPLY(ANSI_COLOR_BLUE, "yielding\n"), PRINT_FLAG_BOTH);
    yield();
  }

  user_trap_ret();
}

g_bool killed(proc_t *p) {
  g_bool killed = false;

  acquire(&p->lock);
  if (p->killed) {
    killed = true;
  }
  release(&p->lock);

  return killed;
}

void sleep(void *chan, struct spinlock *lk) {
  proc_t *p = current_proc();

  acquire(&p->lock);
  release(lk);

  p->chan = chan;
  p->state = SLEEPING;

  sched();

  p->chan = NULL;

  release(&p->lock);
  acquire(lk);
}

uint64_t wait(uint64_t address) {
  proc_t *pp;
  g_bool has_children = false;
  uint64_t pid;
  proc_t *p = current_proc();

  acquire(&wait_lock);

  for (;;) {
    has_children = 0;

    for (uint8_t i = 0; i < NPROC; i++) {
      pp = &proc[i];
      if (pp->parent == p) {

        acquire(&pp->lock);
        has_children = 1;

        if (pp->state == ZOMBIE) {
          pid = pp->pid;
          if (address != 0 &&
              !result_is_ok(copyout(p->pagetable, address, (void *)&p->xstate,
                                    sizeof(p->xstate)))) {
            release(&pp->lock);
            release(&wait_lock);
            return -1;
          }

          free_process(pp);
          release(&pp->lock);
          release(&wait_lock);
          return pid;
        }

        release(&pp->lock);
      }

      if (!has_children || killed(p)) {
        release(&wait_lock);
        return -1;
      }

      sleep(p, &wait_lock);
    }
  }
}

RESULT_TYPE(void) kill(uint64_t pid) {
  proc_t *p;

  for (uint8_t i = 0; i < NPROC; i++) {
    p = &proc[i];
    acquire(&p->lock);
    if (p->pid == pid) {
      p->killed = 1;
      if (p->state == SLEEPING) {
        p->state = RUNNABLE;
      }
      release(&p->lock);
      return RESULT_SUCCESS(0);
    }
    release(&p->lock);
  }

  return RESULT_FAILURE(RESULT_NOT_FOUND);
}

void setkilled(proc_t *p) {
  acquire(&p->lock);
  p->killed = 1;
  release(&p->lock);
}

g_bool proc_grow(proc_t *p, uint64_t bytes) {
  if (!p || bytes == 0)
    return false;
  uint64_t oldsz = p->sz;
  uint64_t newsz = oldsz + bytes;
  newsz = PGROUNDUP(newsz);
  return uvmalloc(p, oldsz, newsz);
}

g_bool proc_shrink(proc_t *p, uint64_t bytes) {
  if (!p || bytes == 0)
    return false;
  uint64_t oldsz = p->sz;
  uint64_t newsz = (bytes >= oldsz) ? 0 : oldsz - bytes;
  newsz = PGROUNDDOWN(newsz);
  return uvmdealloc(p, oldsz, newsz);
}

uint64_t fork(void) {
  uint64_t pid;

  proc_t *p = current_proc();

  result_t rnew_proc = make_proc();
  if (!result_is_ok(rnew_proc)) {
    return -1;
  }

  proc_t *new_proc = (proc_t *)result_unwrap(rnew_proc);

  if (!uvmcopy(p->pagetable, new_proc->pagetable, p->sz)) {
    free_process(new_proc);
    return -1;
  }

  new_proc->sz = p->sz;

  *(new_proc->trapframe) = *(p->trapframe);

  new_proc->trapframe->a0 = 0; // child returns 0

  pid = new_proc->pid;

  release(&new_proc->lock);

  acquire(&wait_lock);
  new_proc->parent = p;
  release(&wait_lock);

  acquire(&new_proc->lock);
  new_proc->state = RUNNABLE;
  release(&new_proc->lock);

  return pid;
}

RESULT_TYPE(void) proc_resize(int n) {
  uint64_t sz;
  proc_t *p = current_proc();

  sz = p->sz;
  if (n > 0) {
    if (proc_grow(p, n) == false) {
      return RESULT_FAILURE(RESULT_ERROR);
    }
  } else if (n < 0) {
    if (proc_shrink(p, -n) == false) {
      return RESULT_FAILURE(RESULT_ERROR);
    }
  } else {
    return RESULT_FAILURE(RESULT_ERROR);
  }
  p->sz = sz;
  return RESULT_SUCCESS(0);
}
