#pragma once

#include <stdint.h>

static inline long sys_print_int(long v) {
  register long a0 asm("a0") = v;
  register long a7 asm("a7") = 0x10; /* SYSCALL_PRINT_INT */
  asm volatile("ecall" : "+r"(a0) : "r"(a7) : "memory");
  return a0;
}

static inline void sys_exit(int status) {
  register long a0 asm("a0") = status;
  register long a7 asm("a7") = 0x02; /* SYSCALL_EXIT */
  asm volatile("ecall" : : "r"(a0), "r"(a7) : "memory");
  __builtin_unreachable();
}

static inline uint32_t sys_notif_register(uint16_t type, uint64_t handler, uint64_t arg, uint32_t flags) {
  register long a0 asm("a0") = (long)type;
  register long a1 asm("a1") = (long)handler;
  register long a2 asm("a2") = (long)arg;
  register long a3 asm("a3") = (long)flags;
  register long a7 asm("a7") = 0x90; /* SYSCALL_NOTIF_REGISTER */
  asm volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a2), "r"(a3), "r"(a7) : "memory");
  return (uint32_t)a0;
}

static inline int sys_notif_unregister(uint16_t type, uint32_t id) {
  register long a0 asm("a0") = (long)type;
  register long a1 asm("a1") = (long)id;
  register long a7 asm("a7") = 0x91; /* SYSCALL_NOTIF_UNREGISTER */
  asm volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a7) : "memory");
  return (int)a0;
}


