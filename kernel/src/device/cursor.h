#pragma once
#include <device/framebuffer.h>
#include <lib/result.h>
#include <lib/types.h>

/* Extremely simple software mouse cursor – square 8×8, white fill, black border
 */
typedef struct {
  framebuffer_t *fb;
  int32_t x; /* top‑left corner */
  int32_t y;
  int32_t px;
  int32_t py;
  uint32_t size; /* width/height (always 8 for now) */
  g_bool is_initialized;
} cursor_t;

RESULT_TYPE(cursor_t *) make_cursor(framebuffer_t *fb);

/* place initial position */
g_bool cursor_init(cursor_t *c, int32_t start_x, int32_t start_y);

/* redraw the cursor at its current position */
void cursor_redraw(cursor_t *c);

/* move by a relative delta and immediately redraw */
void cursor_move(cursor_t *c, int32_t dx, int32_t dy);
void cursor_redraw(cursor_t *c);
