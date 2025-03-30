#pragma once

#include <lib/result.h>
#include <lib/types.h>
#include <limine.h>
#include <stdint.h>

typedef struct {
  struct limine_framebuffer *framebuffer;
  g_bool is_initialized;
} framebuffer_t;

/**
 * Creates a new framebuffer device.
 * @param framebuffer The framebuffer device to create.
 * @return A result_t that can safely be cast to a framebuffer_t pointer if
 * successful.
 */
RESULT_TYPE(framebuffer_t *)
make_framebuffer(struct limine_framebuffer *framebuffer);

/**
 * Initializes the framebuffer device.
 * @param fb The framebuffer device to initialize.
 * @return A g_bool indicating success or failure.
 */
g_bool framebuffer_init(framebuffer_t *fb);

/**
 * Puts a pixel in the framebuffer.
 * @param fb The framebuffer device to use.
 * @param x The x coordinate of the pixel.
 * @param y The y coordinate of the pixel.
 * @param pixel The pixel data (RGB).
 */
void framebuffer_put_pixel(framebuffer_t *fb, uint32_t x, uint32_t y,
                           uint8_t pixel[3]);
