#include "canary.h"

void canary_dbg_val(uint64_t val) {
  // do nothing, just prevent optimization
  asm volatile("" ::"r"(val));
  asm volatile("" ::: "memory");
}
