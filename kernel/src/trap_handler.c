#include "trap_handler.h"
#include "device/plic.h"
#include "device/shared.h"
#include "device/virtio/virtio_mouse.h"
#include "lib/sbi.h"
#include "lib/time.h"
#include "lib/timer.h"
#include "mem_layout.h"
#include "physical_alloc.h"
#include "proc.h"
#include <device/virtio/virtio_keyboard.h>
#include <lib/ansi.h>
#include <lib/cpu.h>
#include <lib/print.h>
#include <lib/str.h>
#include <platform/interrupts.h>
#include <platform/registers.h>
#include <stdint.h>

// #define TICK_INTERVAL_CYCLES 1000000
// #define TICK_INTERVAL_CYCLES 100000
#define TICK_INTERVAL_CYCLES 1000000

static uint64_t next_deadline = 0;

extern void trap_vector();

// Function to get a human-readable cause string
const char *get_exception_cause_str(uint64_t cause) {
  switch (cause) {
  case 0:
    return "Instruction address misaligned";
  case 1:
    return "Instruction access fault";
  case 2:
    return "Illegal instruction";
  case 3:
    return "Breakpoint";
  case 4:
    return "Load address misaligned";
  case 5:
    return "Load access fault";
  case 6:
    return "Store/AMO address misaligned";
  case 7:
    return "Store/AMO access fault";
  case 8:
    return "Environment call from U-mode";
  case 9:
    return "Environment call from S-mode";
  case 10:
    return "Reserved";
  case 11:
    return "Environment call from M-mode";
  case 12:
    return "Instruction page fault";
  case 13:
    return "Load page fault";
  case 14:
    return "Reserved";
  case 15:
    return "Store/AMO page fault";
  default:
    if (cause & (1ULL << 63)) {
      return "Interrupt";
    } else {
      return "Unknown exception";
    }
  }
}

void kernel_trap_handler(trap_regs_t *regs) {
  uint64_t sepc = PS_get_exception_pc();
  uint64_t scause = PS_get_exception_cause();
  uint64_t stval = PS_get_exception_value();
  uint64_t sstatus = PS_get_status();

  if (scause & (1ULL << 63)) {
    // Handle interrupt
    uint64_t interrupt_code = scause & 0x7FFFFFFF;
    handle_interrupt(interrupt_code, sepc);
  } else {
    // Handle exception
    exception_handler(scause, sepc, stval, sstatus, regs);
  }
}

void exception_handler(uint64_t scause, uint64_t sepc, uint64_t stval,
                       uint64_t sstatus, const trap_regs_t *regs) {
  uint64_t cause_code = scause & 0x7FFFFFFFFFFFFFFF;

  if (cause_code == 12 || cause_code == 13 || cause_code == 15) {
    for (uint64_t i = 0; i < NPROC; i++) {
      uint64_t guard_start = KSTACK(i) - PAGE_SIZE;
      uint64_t guard_end = KSTACK(i);
      if (stval >= guard_start && stval < guard_end) {
        const char *acc = (cause_code == 12)   ? "execute"
                          : (cause_code == 13) ? "read"
                                               : "write";
        proc_t *gproc = &processes[i];
        printf(ANSI_APPLY(
                   ANSI_COLOR_RED,
                   "Kernel stack guard page accessed by process pid=%{type: "
                   "int} name=%{type: str}: %{type: str} at 0x%{type: hex} "
                   "(guard [%{type: hex} - %{type: hex}])\n"),
               PRINT_FLAG_BOTH, gproc->pid, gproc->name, acc, stval,
               guard_start, guard_end - 1);
        break;
      }
    }
  }

  print("\n\n", PRINT_FLAG_BOTH);
  print(
      ANSI_APPLY(ANSI_COLOR_RED, ANSI_APPLY(ANSI_EFFECT_BOLD, "TRAP HANDLER")),
      PRINT_FLAG_BOTH);
  print("\nA trap has occurred. This is usually due to an unhandled "
        "exception or a fatal error.\n\n",
        PRINT_FLAG_BOTH);

  // Print exception information
  char buffer[64];

  // Print exception cause
  print(ANSI_APPLY(ANSI_EFFECT_BOLD, "Exception Cause: "), PRINT_FLAG_BOTH);
  hexstrfuint(scause, buffer);
  print("0x", PRINT_FLAG_BOTH);
  print(buffer, PRINT_FLAG_BOTH);
  print(" (", PRINT_FLAG_BOTH);
  print(get_exception_cause_str(scause & 0x7FFFFFFFFFFFFFFF), PRINT_FLAG_BOTH);
  print(")\n", PRINT_FLAG_BOTH);

  // Print instruction pointer where exception occurred
  print(ANSI_APPLY(ANSI_EFFECT_BOLD, "Exception PC: "), PRINT_FLAG_BOTH);
  hexstrfuint(sepc, buffer);
  print("0x", PRINT_FLAG_BOTH);
  print(buffer, PRINT_FLAG_BOTH);
  print("\n", PRINT_FLAG_BOTH);

  // Print bad address or instruction (if applicable)
  print(ANSI_APPLY(ANSI_EFFECT_BOLD, "Trap Value: "), PRINT_FLAG_BOTH);
  hexstrfuint(stval, buffer);
  print("0x", PRINT_FLAG_BOTH);
  print(buffer, PRINT_FLAG_BOTH);
  print("\n", PRINT_FLAG_BOTH);

  // Print status register
  print(ANSI_APPLY(ANSI_EFFECT_BOLD, "Status Register: "), PRINT_FLAG_BOTH);
  hexstrfuint(sstatus, buffer);
  print("0x", PRINT_FLAG_BOTH);
  print(buffer, PRINT_FLAG_BOTH);
  print("\n\n", PRINT_FLAG_BOTH);

  // Print memory stats
  print(ANSI_APPLY(ANSI_EFFECT_BOLD, "Memory Status:\n"), PRINT_FLAG_BOTH);
  strfuint(get_free_page_count(), buffer);
  print("Free page count: ", PRINT_FLAG_BOTH);
  print(buffer, PRINT_FLAG_BOTH);
  print("\n\n", PRINT_FLAG_BOTH);

  // Print register dump
  print(ANSI_APPLY(ANSI_EFFECT_BOLD, "Registers:\n"), PRINT_FLAG_BOTH);
  uint64_t sp_at_trap;
  sp_at_trap = PS_get_scratch();
  printf("ra:  0x%{type: hex}\n", PRINT_FLAG_BOTH, regs->ra);
  printf("sp:  0x%{type: hex}\n", PRINT_FLAG_BOTH, sp_at_trap);
  printf("gp:  0x%{type: hex}\n", PRINT_FLAG_BOTH, regs->gp);
  printf("tp:  0x%{type: hex}\n", PRINT_FLAG_BOTH, regs->tp);
  printf("t0:  0x%{type: hex}  t1:  0x%{type: hex}  t2:  0x%{type: hex}\n",
         PRINT_FLAG_BOTH, regs->t0, regs->t1, regs->t2);
  printf("t3:  0x%{type: hex}  t4:  0x%{type: hex}  t5:  0x%{type: hex}  t6:  "
         "0x%{type: hex}\n",
         PRINT_FLAG_BOTH, regs->t3, regs->t4, regs->t5, regs->t6);
  printf("s0:  0x%{type: hex}  s1:  0x%{type: hex}\n", PRINT_FLAG_BOTH,
         regs->s0, regs->s1);
  printf("s2:  0x%{type: hex}  s3:  0x%{type: hex}  s4:  0x%{type: hex}\n",
         PRINT_FLAG_BOTH, regs->s2, regs->s3, regs->s4);
  printf("s5:  0x%{type: hex}  s6:  0x%{type: hex}  s7:  0x%{type: hex}\n",
         PRINT_FLAG_BOTH, regs->s5, regs->s6, regs->s7);
  printf("s8:  0x%{type: hex}  s9:  0x%{type: hex}  s10: 0x%{type: hex}  s11: "
         "0x%{type: hex}\n",
         PRINT_FLAG_BOTH, regs->s8, regs->s9, regs->s10, regs->s11);
  printf("a0:  0x%{type: hex}  a1:  0x%{type: hex}  a2:  0x%{type: hex}  a3:  "
         "0x%{type: hex}\n",
         PRINT_FLAG_BOTH, regs->a0, regs->a1, regs->a2, regs->a3);
  printf("a4:  0x%{type: hex}  a5:  0x%{type: hex}  a6:  0x%{type: hex}  a7:  "
         "0x%{type: hex}\n",
         PRINT_FLAG_BOTH, regs->a4, regs->a5, regs->a6, regs->a7);
  print("\n", PRINT_FLAG_BOTH);

  // Halt the system (or you could return to let the trap.s code handle it)
  print("System halted.\n", PRINT_FLAG_BOTH);
  for (;;) {
    asm volatile("wfi");
  }
}

void handle_interrupt(uint64_t interrupt_code, uint64_t sepc) {
  uint64_t sstatus_on_entry;
  asm volatile(
      "csrr %0, sstatus"
      : "=r"(sstatus_on_entry)); // Read sstatus as it is upon handler entry

  // --- Debug: Check if interrupts were enabled *before* this trap ---
  uint64_t spie =
      (sstatus_on_entry >> 5) & 1; // Supervisor Previous Interrupt Enable
  uint64_t spp =
      (sstatus_on_entry >> 8) & 1; // Supervisor Previous Privilege (0=U, 1=S)

  if (!spie) {
    printf(ANSI_APPLY(ANSI_COLOR_YELLOW,
                      "WARNING: Interrupt occurred while SIE was disabled "
                      "(SPIE=0)! sret will restore disabled state.\n"),
           PRINT_FLAG_BOTH);
  }

  switch (interrupt_code) {
  case 1: // Supervisor software interrupt
    print("Supervisor software interrupt\n", PRINT_FLAG_BOTH);
    break;
  case 5: // Supervisor timer interrupt
          // print("Supervisor timer interrupt\n", PRINT_FLAG_BOTH);
    // print("Kenrnel encountered a timer interrupt\n", PRINT_FLAG_BOTH);
    print("tick\n", PRINT_FLAG_UART);
    sbi_set_timer(get_csrr_time() + TICK_INTERVAL_CYCLES);
    break;
  case 9: // Supervisor external interrupt
    handle_external_interrupt();
    break;
  default:
    printf("Unknown interrupt: %{type: int}\n", PRINT_FLAG_BOTH,
           interrupt_code);
    break;
  }
}

void handle_external_interrupt() {
  uint32_t irq = shared_plic_claim(0, PLIC_CONTEXT_SUPERVISOR);

  if (irq == 0) { /* spurious or already-handled source   */
    shared_plic_complete(0, PLIC_CONTEXT_SUPERVISOR, 0);
    return;
  }

  // Handle based on IRQ number
  switch (irq) {

  case 10: // UART IRQ
    if (shared_uart_initialized) {
      plic_disable_interrupt(shared_plic, 0, PLIC_CONTEXT_SUPERVISOR, 10);
      volatile uint8_t *u = (uint8_t *)shared_uart->base;

      (void)u[2]; // read IIR ‑‑ clears the IRQ source
      // NOTE: IIR bits 0‑3 give the reason (0b010 = Rx, 0b001 = Tx‑empty, …)

      /* drain any pending RX data so the line doesn’t re‑assert immediately */
      while (true) {
        if (!(u[5] & 0x01)) // LSR bit0: Data‑Ready?
          break;
        char c = u[0];
        char s[2] = {c, '\0'};
      }
      plic_enable_interrupt(shared_plic, 0, PLIC_CONTEXT_SUPERVISOR, 10);
    }
    break;
  case 1:
    virtio_keyboard_handle_irq(shared_virtio_keyboard);
    break;
  case 2:
    virtio_mouse_handle_irq(shared_virtio_mouse);
    break;
  default:
    printf("Unknown external interrupt: %{type: int}\n", PRINT_FLAG_BOTH, irq);
    break;
  }

  // Complete the interrupt handling
  // TODO: active hart id
  if (irq != 0) {
    if (!shared_plic_complete(0, PLIC_CONTEXT_SUPERVISOR, irq)) {
      panic_msg("Failed to complete PLIC interrupt");
    }
  }
}
