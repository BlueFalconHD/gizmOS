#ifndef IMG_H
#define IMG_H

#include "../limine.h"
#include <stdint.h>

#define IMAGE_ALIGN_VERTICAL_TOP 0x0
#define IMAGE_ALIGN_VERTICAL_CENTER 0x1
#define IMAGE_ALIGN_VERTICAL_BOTTOM 0x2
#define IMAGE_ALIGN_HORIZONTAL_LEFT 0x0
#define IMAGE_ALIGN_HORIZONTAL_CENTER 0x1
#define IMAGE_ALIGN_HORIZONTAL_RIGHT 0x2

void draw_image(struct limine_framebuffer *fb, uint32_t x_res, uint32_t y_res, uint32_t *palette, uint8_t *image);

void draw_image_aligned(struct limine_framebuffer *fb, uint32_t x_res, uint32_t y_res, uint32_t *palette, uint8_t *image, uint8_t align_horizontal, uint8_t align_vertical);

void draw_image_scaled(struct limine_framebuffer *fb, uint32_t x_res, uint32_t y_res, uint32_t *palette, uint8_t *image, uint32_t scale);

#endif /* IMG_H */
