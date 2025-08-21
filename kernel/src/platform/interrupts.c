#include "interrupts.h"
#include <stdbool.h>

const char *scause_description(uint64_t scause) {
  bool interrupt = SCAUSE_IS_INTERRUPT(scause);
  uint64_t code = SCAUSE_GET_CODE(scause);

  if (interrupt) {
    if (SCAUSE_INTERRUPT_IS_RESERVED(code)) {
      return "Reserved";
    } else if (SCAUSE_INTERRUPT_IS_PLATFORM_DEFINED(code)) {
      return "Platform-defined";
    }

    switch (code) {
    case SCAUSE_INTERRUPT_SOFTWARE_SUPERVISOR:
      return "Supervisor Software Interrupt";
    case SCAUSE_INTERRUPT_TIMER_SUPERVISOR:
      return "Supervisor Timer Interrupt";
    case SCAUSE_INTERRUPT_EXTERNAL_SUPERVISOR:
      return "Supervisor External Interrupt";
    default:
      return "Unknown/Unhandled";
    }
  } else {
    if (SCAUSE_EXCEPTION_IS_RESERVED(code)) {
      return "Reserved";
    } else if (SCAUSE_EXCEPTION_IS_CUSTOM(code)) {
      return "Custom-defined";
    }

    switch (code) {
    case SCAUSE_EXCEPTION_INSTRUCTION_ADDRESS_MISALIGNED:
      return "Instruction Address Misaligned";
    case SCAUSE_EXCEPTION_INSTRUCTION_ACCESS_FAULT:
      return "Instruction Access Fault";
    case SCAUSE_EXCEPTION_ILLEGAL_INSTRUCTION:
      return "Illegal Instruction";
    case SCAUSE_EXCEPTION_BREAKPOINT:
      return "Breakpoint";
    case SCAUSE_EXCEPTION_LOAD_ADDRESS_MISALIGNED:
      return "Load Address Misaligned";
    case SCAUSE_EXCEPTION_LOAD_ACCESS_FAULT:
      return "Load Access Fault";
    case SCAUSE_EXCEPTION_STORE_AMO_ADDRESS_MISALIGNED:
      return "Store/AMO Address Misaligned";
    case SCAUSE_EXCEPTION_STORE_AMO_ACCESS_FAULT:
      return "Store/AMO Access Fault";
    case SCAUSE_EXCEPTION_ENVIRONMENT_CALL_FROM_U_MODE:
      return "Environment Call from U-mode";
    case SCAUSE_EXCEPTION_ENVIRONMENT_CALL_FROM_S_MODE:
      return "Environment Call from S-mode";
    case SCAUSE_EXCEPTION_INSTRUCTION_PAGE_FAULT:
      return "Instruction Page Fault";
    case SCAUSE_EXCEPTION_LOAD_PAGE_FAULT:
      return "Load Page Fault";
    case SCAUSE_EXCEPTION_STORE_AMO_PAGE_FAULT:
      return "Store/AMO Page Fault";
    default:
      return "Unknown/Unhandled";
    }
  }

  return "Unknown/Unhandled";
}
