#include "gfx.h"
#include "device/framebuffer.h"
#include "device/shared.h"

#include <stdint.h>

void fill_screen_with_color(uint8_t r, uint8_t g, uint8_t b) {
  if (!shared_framebuffer_initialized) {
    return;
  }

  framebuffer_t *fb = shared_framebuffer;
  uint8_t panic_color[3] = {r, g, b};
  for (uint32_t y = 0; y < fb->framebuffer->height; y++) {
    for (uint32_t x = 0; x < fb->framebuffer->width; x++) {
      framebuffer_put_pixel(fb, x, y, panic_color);
    }
  }
}
