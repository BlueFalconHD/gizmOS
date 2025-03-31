#pragma once

#include <stdint.h>

void trap_handler();
void exception_handler(uint64_t scause, uint64_t sepc, uint64_t stval,
                       uint64_t sstatus);
void handle_interrupt(uint64_t interrupt_code, uint64_t sepc);
void handle_external_interrupt();
