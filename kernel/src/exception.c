#include <lib/panic.h>
#include <stdint.h>

void handle_exception_sync(uint64_t esr) {
  panic("Synchronous exception occurred\n");
}

void handle_exception_irq(uint64_t esr) { panic("IRQ exception occurred\n"); }

void handle_exception_fiq(uint64_t esr) { panic("FIQ exception occurred\n"); }

void handle_exception_serr(uint64_t esr) {
  panic("SError exception occurred\n");
}
