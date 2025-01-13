#include "img.h"
#include "../limine.h"
#include "../device/framebuffer.h"

void draw_image(struct limine_framebuffer *fb, uint32_t x_res, uint32_t y_res, uint32_t *palette, uint8_t *image) {
    for (uint32_t x = 0; x < x_res; x++) {
        for (uint32_t y = 0; y < y_res; y++) {
            uint32_t pixel = palette[image[y * x_res + x]];
            write_rgb256_pixel(fb, x, y, (uint8_t *)&pixel);
        }
    }
}

void draw_image_aligned(struct limine_framebuffer *fb, uint32_t x_res, uint32_t y_res, uint32_t *palette, uint8_t *image, uint8_t align_horizontal, uint8_t align_vertical) {
    // get origin x and y
    if (align_horizontal == IMAGE_ALIGN_HORIZONTAL_CENTER) {
        align_horizontal = (fb->width - x_res) / 2;
    } else if (align_horizontal == IMAGE_ALIGN_HORIZONTAL_RIGHT) {
        align_horizontal = fb->width - x_res;
    } else {
        align_horizontal = 0;
    }

    if (align_vertical == IMAGE_ALIGN_VERTICAL_CENTER) {
        align_vertical = (fb->height - y_res) / 2;
    } else if (align_vertical == IMAGE_ALIGN_VERTICAL_BOTTOM) {
        align_vertical = fb->height - y_res;
    } else {
        align_vertical = 0;
    }

    for (uint32_t x = 0; x < x_res; x++) {
        for (uint32_t y = 0; y < y_res; y++) {
            uint32_t pixel = palette[image[y * x_res + x]];
            write_rgb256_pixel(fb, align_horizontal + x, align_vertical + y, (uint8_t *)&pixel);
        }
    }
}

void draw_image_scaled(struct limine_framebuffer *fb, uint32_t x_res, uint32_t y_res, uint32_t *palette, uint8_t *image, uint32_t scale) {
    for (uint32_t x = 0; x < x_res; x++) {
        for (uint32_t y = 0; y < y_res; y++) {
            uint32_t pixel = palette[image[y * x_res + x]];
            for (uint32_t i = 0; i < scale; i++) {
                for (uint32_t j = 0; j < scale; j++) {
                    write_rgb256_pixel(fb, x * scale + i, y * scale + j, (uint8_t *)&pixel);
                }
            }
        }
    }
}
