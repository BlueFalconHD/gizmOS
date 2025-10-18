#pragma once

#include <lib/macros.h>
#include <stdint.h>

#define MHZ(x) ((x) * 1000000)
#define TIMER_FREQUENCY MHZ(10)

#ifndef TICK_HZ
#define TICK_HZ 250
#endif

#define TICK_INTERVAL_CYCLES ((uint64_t)(TIMER_FREQUENCY) / (uint64_t)(TICK_HZ))

G_INLINE uint64_t get_csrr_time(void) {
  uint64_t t;
  asm volatile("csrr %0, time" : "=r"(t));
  return t;
}

G_INLINE uint64_t get_time_in_cycles(void) {
  uint64_t cycles;
  asm volatile("rdcycle %0" : "=r"(cycles));
  return cycles;
}
