#include "canary.h"

// store canary value in
void canary_dbg_val(uint64_t val) {
  __asm__ volatile("mv t6, %0" ::"r"(val) : "x31");
  asm volatile("" ::: "memory");
}
