#include "cursor.h"
#include "device/shared.h"
#include "img/cursor.h"
#include "img/img.h"
#include <physical_alloc.h>

#define CURSOR_SZ LOGINUI_CURSOR_2X_WIDTH

RESULT_TYPE(cursor_t *) make_cursor(framebuffer_t *fb) {
  cursor_t *c = (cursor_t *)alloc_page();
  if (!c)
    return RESULT_FAILURE(RESULT_NOMEM);

  c->fb = fb;
  c->x = 0;
  c->y = 0;
  c->px = 0;
  c->py = 0;
  c->size = CURSOR_SZ;
  c->is_initialized = false;
  return RESULT_SUCCESS(c);
}

g_bool cursor_init(cursor_t *c, int32_t start_x, int32_t start_y) {
  if (!c || !c->fb)
    return false;

  c->x = start_x;
  c->y = start_y;
  c->is_initialized = true;
  return true;
}

void cursor_move(cursor_t *c, int32_t dx, int32_t dy) {
  if (!c || !c->is_initialized)
    return;

  c->px = c->x;
  c->py = c->y;

  c->x += dx;
  c->y += dy;

  /* clamp into visible area */
  if (c->x < 0)
    c->x = 0;
  if (c->y < 0)
    c->y = 0;
  if (c->x > (int32_t)c->fb->framebuffer->width - (int32_t)c->size)
    c->x = (int32_t)c->fb->framebuffer->width - (int32_t)c->size;
  if (c->y > (int32_t)c->fb->framebuffer->height - (int32_t)c->size)
    c->y = (int32_t)c->fb->framebuffer->height - (int32_t)c->size;

  /* erase old cursor */
  fill_area(shared_framebuffer, c->px, c->py, LOGINUI_CURSOR_2X_WIDTH,
            LOGINUI_CURSOR_2X_HEIGHT, 0);

  draw_image_no_palette_location(shared_framebuffer, LOGINUI_CURSOR_2X_WIDTH,
                                 LOGINUI_CURSOR_2X_HEIGHT, loginui_cursor_2x,
                                 c->x, c->y);
}
