#include "panic.h"
#include "device/uart.h"
#include <device/framebuffer.h>
#include <device/term.h>
#include <font/font_render.h>
#include <hhdm.h>
#include <lib/ansi.h>
#include <lib/print.h>

void panic(const char *msg) {
  panic_msg(msg);

  for (;;) {
    // hcf loop, noop
    asm("nop");
  }
}

void panic_msg(const char *msg) {
  print(ANSI_EFFECT_BOLD, PRINT_FLAG_BOTH);
  print(ANSI_RGB_COLOR("231", "130", "132"), PRINT_FLAG_BOTH);
  print("PANIC: ", PRINT_FLAG_BOTH);
  print(ANSI_EFFECT_RESET, PRINT_FLAG_BOTH);
  print(msg, PRINT_FLAG_BOTH);
  print("\n", PRINT_FLAG_BOTH);
}

void panic_msg_no_cr(const char *msg) {
  term_puts(ANSI_EFFECT_BOLD);
  term_puts(ANSI_RGB_COLOR("231", "130", "132"));
  term_puts("PANIC: ");
  term_puts(ANSI_EFFECT_RESET);
  term_puts(msg);
}

void panic_lastresort(const char *msg) {
  uart_init();
  uart_puts("PANIC: ");
  uart_puts(msg);
  uart_puts("\n");

  for (;;) {
    asm("nop");
  }
}
