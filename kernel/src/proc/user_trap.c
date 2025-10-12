#include "user_trap.h"
#include "scheduler.h"
#include "process.h"
#include <lib/cpu.h>
#include <lib/panic.h>
#include <lib/gfx.h>
#include <lib/print.h>
#include <lib/gizm_font.h>
#include <mem_layout.h>
#include <page_table.h>
#include <platform/interrupts.h>
#include <platform/registers.h>

extern char trampoline[];
extern char uservec[];
extern char userret[];
extern void trap_vector();
extern uint64_t hhdm_offset;

void user_trap_ret(void) {
  proc_t *p = current_proc();
  PS_disable_interrupts();

  if (p->is_kernel) {
    PS_set_trap_vector((uint64_t)trap_vector);
    PS_enable_interrupts();
    return;
  }

  uint64_t trampoline_uservec = TRAMPOLINE + (uservec - trampoline);
  PS_set_trap_vector(trampoline_uservec);

  p->trapframe->kernel_satp = PS_get_atp();
  p->trapframe->kernel_sp = p->kstack + KSTACK_PAGES * PAGE_SIZE;
  p->trapframe->kernel_trap = (uint64_t)usertrap;
  p->trapframe->kernel_hartid = P_get_thread_ptr();

  uint64_t x = PS_get_status();
  x &= ~SSTATUS_SPP;
  x |= SSTATUS_SPIE;
  PS_set_status(x);

  PS_set_exception_pc(p->trapframe->epc);

  uint64_t table_pa = ((uint64_t)p->pagetable) - hhdm_offset;
  uint64_t table_ppn = table_pa >> 12;
  uint64_t satp_value = (uint64_t)8ULL << 60;
  satp_value |= table_ppn;

  uint64_t trampoline_userret = TRAMPOLINE + (userret - trampoline);
  ((void (*)(uint64_t))trampoline_userret)(satp_value);
}

void forkret() {
  release(&current_proc()->lock);
  user_trap_ret();
}

void usertrap(void) {
  if ((PS_get_status() & SSTATUS_SPP) != 0)
    panic("usertrap: not from user mode");

  PS_set_trap_vector((uint64_t)trap_vector);

  proc_t *p = current_proc();

  if (PS_get_exception_cause() == 0x2) {
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

  p->trapframe->epc = PS_get_exception_pc();

  if (PS_get_exception_cause() == 8) {
    PS_enable_interrupts();

    int callnum = p->trapframe->a7;
    if (callnum == 2) {
      // exit
    } else if (callnum == 6) {
      fill_screen_with_color(25, 25, 25);
    } else if (callnum == 7) {
      gizm_font_draw_text(20, 20, "Proc A", GIZM_COLOR_BLUE);
    } else if (callnum == 8) {
      gizm_font_draw_text(20, 20, "Proc B", GIZM_COLOR_RED);
    }
  }

  if (PS_get_exception_cause() == 0x8000000000000005) {
    yield();
  }

  user_trap_ret();
}


