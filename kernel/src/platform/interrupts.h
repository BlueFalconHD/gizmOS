#pragma once

#include "registers.h"
#include <lib/macros.h>

#define SSTATUS_SPP (1L << 8)  // Previous mode, 1=Supervisor, 0=User
#define SSTATUS_SPIE (1L << 5) // Supervisor Previous Interrupt Enable
#define SSTATUS_UPIE (1L << 4) // User Previous Interrupt Enable
#define SSTATUS_SIE (1L << 1)  // Supervisor Interrupt Enable
#define SSTATUS_UIE (1L << 0)  // User Interrupt Enable
#define SSTATUS_SUM (1L << 18) // Supervisor User Memory Access

#define SIE_EXTERNAL (1 << 9) // External interrupt enable bit in sie register
#define SIE_TIMER (1 << 5)    // Timer interrupt enable bit in sie register
#define SIE_SOFTWARE (1 << 1) // Software interrupt enable bit in sie register
#define SIE_ALL                                                                \
  (SIE_EXTERNAL | SIE_TIMER | SIE_SOFTWARE) // All interrupt enable bits

#define SCAUSE_MASK_INTERRUPT (1UL << 63)
#define SCAUSE_MASK_CODE (0x7FFFFFFFFFFFFFFFUL)
#define SCAUSE_IS_INTERRUPT(scause) (((scause) & SCAUSE_MASK_INTERRUPT) >> 63)
#define SCAUSE_GET_CODE(scause) ((scause) & SCAUSE_MASK_CODE)

#define SCAUSE_INTERRUPT_IS_RESERVED(int_code)                                 \
  ((int_code) == 0 || ((int_code) >= 2 && (int_code) <= 4) ||                  \
   ((int_code) >= 6 && (int_code) <= 8) ||                                     \
   ((int_code) >= 10 && (int_code) <= 15))
#define SCAUSE_INTERRUPT_IS_PLATFORM_DEFINED(int_code) (int_code >= 16)
#define SCAUSE_INTERRUPT_SOFTWARE_SUPERVISOR 1
#define SCAUSE_INTERRUPT_TIMER_SUPERVISOR 5
#define SCAUSE_INTERRUPT_EXTERNAL_SUPERVISOR 9

#define SCAUSE_EXCEPTION_IS_RESERVED(exc_code)                                 \
  ((exc_code) == 10 || (exc_code) == 11 || (exc_code) == 14 ||                 \
   ((exc_code) >= 16 && (exc_code) <= 23) ||                                   \
   ((exc_code) >= 32 && (exc_code) <= 47) || (exc_code) >= 64)
#define SCAUSE_EXCEPTION_IS_CUSTOM(exc_code)                                   \
  (((exc_code) >= 24 && (exc_code) <= 31) ||                                   \
   ((exc_code) >= 48 && (exc_code) <= 63))
#define SCAUSE_EXCEPTION_INSTRUCTION_ADDRESS_MISALIGNED 0
#define SCAUSE_EXCEPTION_INSTRUCTION_ACCESS_FAULT 1
#define SCAUSE_EXCEPTION_ILLEGAL_INSTRUCTION 2
#define SCAUSE_EXCEPTION_BREAKPOINT 3
#define SCAUSE_EXCEPTION_LOAD_ADDRESS_MISALIGNED 4
#define SCAUSE_EXCEPTION_LOAD_ACCESS_FAULT 5
#define SCAUSE_EXCEPTION_STORE_AMO_ADDRESS_MISALIGNED 6
#define SCAUSE_EXCEPTION_STORE_AMO_ACCESS_FAULT 7
#define SCAUSE_EXCEPTION_ENVIRONMENT_CALL_FROM_U_MODE 8
#define SCAUSE_EXCEPTION_ENVIRONMENT_CALL_FROM_S_MODE 9
#define SCAUSE_EXCEPTION_INSTRUCTION_PAGE_FAULT 12
#define SCAUSE_EXCEPTION_LOAD_PAGE_FAULT 13
#define SCAUSE_EXCEPTION_STORE_AMO_PAGE_FAULT 15

G_INLINE void PS_enable_interrupts(void) {
  uint64_t sstatus = PS_get_status();
  sstatus |= SSTATUS_SIE;
  PS_set_status(sstatus);
}

G_INLINE void PS_disable_interrupts(void) {
  uint64_t sstatus = PS_get_status();
  sstatus &= ~SSTATUS_SIE;
  PS_set_status(sstatus);
}

G_INLINE uint64_t PS_get_interrupt_enabled(void) {
  return PS_get_status() & SSTATUS_SIE;
}

G_INLINE void PS_enable_interrupt_type(uint64_t interrupt) {
  uint64_t sie = PS_get_interrupt_enable();
  sie |= interrupt;
  PS_set_interrupt_enable(sie);
}

G_INLINE void PS_disable_interrupt_type(uint64_t interrupt) {
  uint64_t sie = PS_get_interrupt_enable();
  sie &= ~interrupt;
  PS_set_interrupt_enable(sie);
}

G_INLINE void PS_enable_all_interrupt_types(void) {
  PS_enable_interrupt_type(SIE_ALL);
}

G_INLINE void PS_disable_all_interrupts_types(void) {
  PS_disable_interrupt_type(SIE_ALL);
}

const char *scause_description(uint64_t scause);
