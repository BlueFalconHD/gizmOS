#include "img.h"
#include <device/framebuffer.h>
#include <limine.h>

void draw_image(framebuffer_t *fb, uint32_t x_res, uint32_t y_res,
                uint32_t *palette, uint8_t *image) {
  for (uint32_t x = 0; x < x_res; x++) {
    for (uint32_t y = 0; y < y_res; y++) {
      uint32_t pixel = palette[image[y * x_res + x]];
      uint8_t pixel_data[3];
      pixel_data[0] = (pixel >> 16) & 0xFF; // Red
      pixel_data[1] = (pixel >> 8) & 0xFF;  // Green
      pixel_data[2] = pixel & 0xFF;         // Blue
      framebuffer_put_pixel(fb, x, y, pixel_data);
    }
  }
}

void draw_image_aligned(framebuffer_t *fb, uint32_t x_res, uint32_t y_res,
                        uint32_t *palette, uint8_t *image,
                        uint8_t align_horizontal, uint8_t align_vertical) {
  // get origin x and y
  if (align_horizontal == IMAGE_ALIGN_HORIZONTAL_CENTER) {
    align_horizontal = (fb->framebuffer->width - x_res) / 2;
  } else if (align_horizontal == IMAGE_ALIGN_HORIZONTAL_RIGHT) {
    align_horizontal = fb->framebuffer->width - x_res;
  } else {
    align_horizontal = 0;
  }

  if (align_vertical == IMAGE_ALIGN_VERTICAL_CENTER) {
    align_vertical = (fb->framebuffer->height - y_res) / 2;
  } else if (align_vertical == IMAGE_ALIGN_VERTICAL_BOTTOM) {
    align_vertical = fb->framebuffer->height - y_res;
  } else {
    align_vertical = 0;
  }

  for (uint32_t x = 0; x < x_res; x++) {
    for (uint32_t y = 0; y < y_res; y++) {
      uint32_t pixel = palette[image[y * x_res + x]];
      uint8_t pixel_data[3];
      pixel_data[0] = (pixel >> 16) & 0xFF; // Red
      pixel_data[1] = (pixel >> 8) & 0xFF;  // Green
      pixel_data[2] = pixel & 0xFF;         // Blue
      framebuffer_put_pixel(fb, x + align_horizontal, y + align_vertical,
                            pixel_data);
    }
  }
}

void draw_image_scaled(framebuffer_t *fb, uint32_t x_res, uint32_t y_res,
                       uint32_t *palette, uint8_t *image, uint32_t scale) {
  for (uint32_t x = 0; x < x_res; x++) {
    for (uint32_t y = 0; y < y_res; y++) {
      uint32_t pixel = palette[image[y * x_res + x]];
      for (uint32_t i = 0; i < scale; i++) {
        for (uint32_t j = 0; j < scale; j++) {
          framebuffer_put_pixel(fb, x * scale + i, y * scale + j,
                                (uint8_t *)&pixel);
        }
      }
    }
  }
}
