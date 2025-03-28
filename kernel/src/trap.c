#include "trap.h"
#include "lib/panic.h"
#include "lib/str.h"

void trap_handler() {
  uint64_t scause, sepc, stval;
  asm volatile("csrr %0, scause" : "=r"(scause));
  asm volatile("csrr %0, sepc" : "=r"(sepc));
  asm volatile("csrr %0, stval" : "=r"(stval));

  // Output the values using term_puts or other available methods
  char buffer[64];
  panic_msg("Trap occurred!\n");

  term_puts("scause: ");
  hexstrfuint(scause, buffer);
  term_puts(buffer);
  term_puts("\n");

  term_puts("sepc: ");
  hexstrfuint(sepc, buffer);
  term_puts(buffer);
  term_puts("\n");

  term_puts("stval: ");
  hexstrfuint(stval, buffer);
  term_puts(buffer);
  term_puts("\n");

  // Halt or reset the processor, or attempt to recover if possible
  while (1) {
    asm volatile("wfi");
  }
}
