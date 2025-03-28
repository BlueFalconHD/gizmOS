#pragma once

#include <stdint.h>

extern uint64_t goldfish_get_time();
extern uint64_t goldfish_get_alarm();
extern uint64_t goldfish_set_alarm(uint64_t alarm);
extern uint64_t goldfish_get_clear_interrupt();
extern uint64_t goldfish_set_clear_interrupt(uint64_t interrupt);

void rtc_mmio_callback(uint64_t addr);
