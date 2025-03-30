#include "trap_handler.h"
#include "physical_alloc.h"
#include <device/term.h>
#include <lib/ansi.h>
#include <lib/print.h>
#include <lib/str.h>

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

void trap_handler() {
  // Read exception registers
  uint64_t sepc, scause, stval, sstatus;

  asm volatile("csrr %0, sepc" : "=r"(sepc));
  asm volatile("csrr %0, scause" : "=r"(scause));
  asm volatile("csrr %0, stval" : "=r"(stval));
  asm volatile("csrr %0, sstatus" : "=r"(sstatus));

  // Print trap header
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

  // You could also dump register values here if you save them in your trap
  // entry point However, that would require modifying your trap.s assembly file

  // Halt the system (or you could return to let the trap.s code handle it)
  print("System halted.\n", PRINT_FLAG_BOTH);
  for (;;) {
    asm volatile("wfi");
  }
}
