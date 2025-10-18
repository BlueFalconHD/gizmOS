#pragma once

#include <lib/macros.h>

/*
 * Forces an exception by accessing 0x0. Assumes this is unmapped (which is
 * standard behaviour currently).
 */
G_INLINE void force_exception() {
  volatile int *ptr = (int *)0x0;
  volatile int value = *ptr;
  (void)value;
}

/*
 * Causes a RISC-V breakpoint exception
 */
G_INLINE void force_breakpoint_exception() { asm volatile("ebreak"); }
