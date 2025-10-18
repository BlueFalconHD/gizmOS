#include "canary.h"

#include <lib/debug.h>

void canary_dbg_val(uint64_t val) {
  // do nothing, just prevent optimization
  asm volatile("" ::"r"(val));
  asm volatile("" ::: "memory");

  dbg(u8"🕊️"); // zed renders this wrong, it is a dove emoji tho
}
