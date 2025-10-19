#include "console.h"
#include <device/framebuffer.h>
#include <extern/flanterm/src/flanterm.h>
#include <extern/flanterm/src/flanterm_backends/fb.h>
#include <lib/kalloc.h>
#include <lib/str.h>
#include <limine.h>

// internal malloc/free for flanterm
//     void *(*_malloc)(size_t size),
//     void (*_free)(void *ptr, size_t size),
static void *flanterm_kalloc(size_t size) { return kalloc(size); }
static void flanterm_kfree(void *ptr, size_t size) { kfree(ptr); }

result_t make_console(framebuffer_t *framebuffer) {
  console_t *console = (console_t *)kalloc(sizeof(console_t));
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
      flanterm_kalloc, flanterm_kfree, framebuffer->address, framebuffer->width,
      framebuffer->height, framebuffer->pitch, framebuffer->red_mask_size,
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

  // ignore errors on next line
#pragma GCC diagnostic push
#pragma clang diagnostic push
  flanterm_write(console->flanterm_ctx, &c, 1);
#pragma GCC diagnostic pop
#pragma clang diagnostic pop
}

void console_puts(console_t *console, const char *s) {
  if (!console->is_initialized) {
    return;
  }

  flanterm_write(console->flanterm_ctx, s, strlen(s));
}
