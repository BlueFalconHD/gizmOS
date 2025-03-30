#include "console.h"
#include <device/framebuffer.h>
#include <extern/flanterm/backends/fb.h>
#include <extern/flanterm/flanterm.h>
#include <lib/str.h>
#include <limine.h>
#include <physical_alloc.h>

result_t make_console(framebuffer_t *framebuffer) {
  console_t *console = (console_t *)alloc_page();
  if (!console) {
    return RESULT_FAILURE(RESULT_NOMEM);
  }

  console->fb = framebuffer;
  console->is_initialized = false;

  return RESULT_SUCCESS(console);
}

g_bool console_init(console_t *console) {
  if (!console) {
    return false;
  }

  if (console->is_initialized) {
    return true;
  }

  struct limine_framebuffer *framebuffer = console->fb->framebuffer;
  if (!framebuffer) {
    return false;
  }

  console->flanterm_ctx = flanterm_fb_init(
      NULL, NULL, framebuffer->address, framebuffer->width, framebuffer->height,
      framebuffer->pitch, framebuffer->red_mask_size,
      framebuffer->red_mask_shift, framebuffer->green_mask_size,
      framebuffer->green_mask_shift, framebuffer->blue_mask_size,
      framebuffer->blue_mask_shift, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
      NULL, 0, 0, 1, 0, 0, 0);

  console->is_initialized = true;

  return true;
}

void console_putc(console_t *console, g_char c) {
  if (!console->is_initialized) {
    return;
  }

  flanterm_write(console->flanterm_ctx, &c, 1);
}

void console_puts(console_t *console, const char *s) {
  if (!console->is_initialized) {
    return;
  }

  flanterm_write(console->flanterm_ctx, s, strlen(s));
}
