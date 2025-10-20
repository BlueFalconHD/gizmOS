#pragma once

#include <stddef.h>
#include <stdint.h>

#include <device/framebuffer.h>
#include <lib/PixelCore/surface.h>
#include <lib/types.h>

/*
 * Minimal PPM (P6) loader and blitters
 * - Supports binary PPM (P6), maxval 255, with comments and whitespace.
 * - No compression; data is width*height*3 bytes in RGB order.
 * - Alpha is set to 0xFF when converting to 32-bit pixels.
 */

typedef struct {
  uint32_t width;
  uint32_t height;
  uint32_t maxval;           /* only 255 is supported */
  const uint8_t *pixel_data; /* pointer into supplied buffer (no copy) */
  size_t pixel_data_size;    /* size in bytes of pixel_data */
} ppm_info_t;

/* Parse a P6 PPM image from memory. Returns true on success. */
g_bool ppm_parse(const void *data, size_t size, ppm_info_t *out);

/* Blit the parsed PPM directly onto a framebuffer at (dst_x, dst_y). */
g_bool ppm_blit_to_framebuffer(framebuffer_t *fb, uint32_t dst_x,
                               uint32_t dst_y, const void *data, size_t size);

/* Blit the parsed PPM into a PixelCore surface at (dst_x, dst_y). */
g_bool ppm_blit_to_surface(PCSurface *surface, int32_t dst_x, int32_t dst_y,
                           const void *data, size_t size);

/* Convenience: Create a surface sized to the PPM and load it. */
PCSurface *ppm_create_surface(const void *data, size_t size, uint32_t z_index);
