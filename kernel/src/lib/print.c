#include "print.h"
#include <device/term.h>
#include <device/uart.h>

void print(const char *str, print_flags_t flags) {
  if (flags & PRINT_FLAG_TERM) {
    term_puts(str);
  }
  if (flags & PRINT_FLAG_UART) {
    uart_puts(str);
  }
}
