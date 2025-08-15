#pragma once

#include <lib/macros.h>
#include <stdint.h>

static inline void tlb_flush_all(void) {
  asm volatile("sfence.vma zero, zero" ::: "memory");
}
