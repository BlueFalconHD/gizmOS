#pragma once

#include <stdint.h>

typedef struct trap_regs {
  uint64_t ra;
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
  uint64_t tp;
  uint64_t gp;
} trap_regs_t;

void trap_handler();
void kernel_trap_handler(trap_regs_t *regs);
void exception_handler(uint64_t scause, uint64_t sepc, uint64_t stval,
                       uint64_t sstatus, const trap_regs_t *regs);
void handle_interrupt(uint64_t interrupt_code, uint64_t sepc);
void handle_external_interrupt();
