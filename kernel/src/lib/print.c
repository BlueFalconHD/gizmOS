#include "print.h"
#include "device/shared.h"
#include <device/term.h>
#include <device/uart.h>

void print(const char *str, print_flags_t flags) {
  if (flags & PRINT_FLAG_TERM) {
    if (shared_console_initialized) {
      console_puts(shared_console, str);
    }
  }
  if (flags & PRINT_FLAG_UART) {
    if (shared_uart_initialized) {
      uart_puts(shared_uart, str);
    }
  }
}
