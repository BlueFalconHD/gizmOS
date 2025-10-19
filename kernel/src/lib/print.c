#include "print.h"
#include "device/shared.h"
#include <device/uart.h>
#include <lib/spinlock.h>

static struct spinlock print_lock = {
    .locked = 0,
    .name = "print",
    .cpu = 0,
};

void print(const char *str, print_flags_t flags) {
  // Prevent flanterm reentrancy by serializing terminal output.
  // If already holding the lock (e.g., printing from a trap while inside
  // a print), skip terminal output to avoid recursion and still allow UART.
  g_bool already_holding = holding(&print_lock);

  if (!already_holding) {
    acquire(&print_lock);
  }

  if ((flags & PRINT_FLAG_TERM) && !already_holding) {
    if (shared_console_initialized) {
      console_puts(shared_console, str);
    }
  }

  if (flags & PRINT_FLAG_UART) {
    if (shared_uart_initialized) {
      uart_puts(shared_uart, str);
    }
  }

  if (!already_holding) {
    release(&print_lock);
  }
}
