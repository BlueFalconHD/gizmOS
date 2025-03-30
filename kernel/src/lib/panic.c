#include "panic.h"
#include "device/shared.h"
#include <device/framebuffer.h>
#include <lib/ansi.h>
#include <lib/print.h>

#define PANIC_BG_COLOR_R 75
#define PANIC_BG_COLOR_G 0
#define PANIC_BG_COLOR_B 130

void fill_fb_with_panic_color() {
  if (!shared_framebuffer_initialized) {
    // screwed
    return;
  }

  framebuffer_t *fb = shared_framebuffer;
  uint8_t panic_color[3] = {PANIC_BG_COLOR_R, PANIC_BG_COLOR_G,
                            PANIC_BG_COLOR_B};
  for (uint32_t y = 0; y < fb->framebuffer->height; y++) {
    for (uint32_t x = 0; x < fb->framebuffer->width; x++) {
      framebuffer_put_pixel(fb, x, y, panic_color);
    }
  }
}

void panic(const char *msg) {
  if (!is_shared_char_available()) {
    fill_fb_with_panic_color();
  }

  panic_msg(msg);

  for (;;) {
    asm("wfi");
  }
}

void panic_location_internal(const char *msg, const char *file, int line) {
  if (!is_shared_char_available()) {
    fill_fb_with_panic_color();
  }

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
