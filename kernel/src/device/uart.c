#include "uart.h"
#include <device/term.h>
#include <lib/str.h>

void uart_puts(const char *s) {
  while (*s) {
#if UART_MIRROR_TO_TERM
    term_putc(*s);
#endif
    uart_putc(*s++);
  }
}

void uart_mmio_callback(uint64_t addr) { uart_init(); }
