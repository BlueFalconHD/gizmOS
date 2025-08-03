#include "cursor_daemon.h"
#include <device/shared.h>
#include <lib/time.h>
#include <proc.h>

void cursor_daemon(void *arg) {
  (void)arg;
  const uint64_t frame_us =
      1000000 / 10; /* 10 Hz - cursor doesn't need constant redraw */

  while (1) {
    // Cursor is redrawn automatically when it moves via cursor_move()
    // Only occasionally redraw to ensure it stays visible
    if (shared_cursor_initialized)
      cursor_redraw(shared_cursor);

    sleep_us(frame_us);
    yield(); /* let others run */
  }
}
