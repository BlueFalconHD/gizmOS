#pragma once

#include <device/framebuffer.h>
#include <lib/result.h>
#include <lib/types.h>
#include <stdint.h>

typedef struct {
  struct flanterm_context *flanterm_ctx;
  framebuffer_t *fb;
  g_bool is_initialized;
} console_t;

/**
 * Creates a new console device.
 * @param framebuffer The framebuffer device to create.
 * @return A result_t that can safely be cast to a console_t pointer if
 * successful.
 */
RESULT_TYPE(console_t *) make_console(framebuffer_t *framebuffer);

/**
 * Initializes the console device.
 * @param console The console device to initialize.
 * @return A g_bool indicating success or failure.
 */
g_bool console_init(console_t *console);

/**
 * Puts a character in the console.
 * @param console The console device to use.
 * @param c The character to put.
 */
void console_putc(console_t *console, g_char c);

/**
 * Puts a string in the console.
 * @param console The console device to use.
 * @param s The string to put.
 */
void console_puts(console_t *console, const char *s);
