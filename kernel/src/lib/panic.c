#include "panic.h"
#include <device/framebuffer.h>
#include <device/term.h>
#include <hhdm.h>
#include <lib/ansi.h>
#include <lib/print.h>

void panic(const char *msg) {
  panic_msg(msg);

  for (;;) {
    asm("wfi");
  }
}

void panic_location_internal(const char *msg, const char *file, int line) {
  printf("[%{type: str}:%{type: int}] ", PRINT_FLAG_BOTH, file, line);
  panic_msg(msg);

  for (;;) {
    asm("wfi");
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
  print(ANSI_EFFECT_BOLD, PRINT_FLAG_BOTH);
  print(ANSI_RGB_COLOR("231", "130", "132"), PRINT_FLAG_BOTH);
  print("PANIC: ", PRINT_FLAG_BOTH);
  print(ANSI_EFFECT_RESET, PRINT_FLAG_BOTH);
  print(msg, PRINT_FLAG_BOTH);
}
