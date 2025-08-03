#include "wallpaper_daemon.h"
#include "lib/gfx.h"
#include <device/shared.h>
#include <lib/time.h>
#include <proc.h>

void wallpaper_daemon(void *arg) {
  (void)arg;
  const uint64_t frame_us = 1000000 / 30; /* 30 Hz */

  while (1) {
    if (shared_framebuffer_initialized)
      fill_screen_with_color(24, 24, 24);

    sleep_us(frame_us);
    yield(); /* let others run */
  }
}
