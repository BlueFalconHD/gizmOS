#ifndef CE_FONT_H
#define CE_FONT_H

#include <stdint.h>
#include <stddef.h>
#include "../limine.h"
#include "../device/framebuffer.h"

typedef struct
{
    size_t len;
    uint8_t const *ptr;
} ce_buf;

typedef struct
{
    size_t width;
    size_t height;
    ce_buf buf;
} ce_font;

extern ce_font CE_SOURCE_CODE_PRO;
extern ce_font CE_SOURCE_CODE_PRO_RETINA;

static inline uint8_t ce_font_sample(ce_font font, char c, uint8_t x, uint8_t y)
{
    if (c < 32 || c > 126)
        return 0;
    c -= 32;
    uint8_t const *pixels = font.buf.ptr;
    return pixels[(c * font.width * font.height) + (y * font.width + x)];
}

// value at location x, y in character c is greyscale
// draw full character at x, y
static inline void ce_font_draw_char(ce_font font, char c, uint8_t x, uint8_t y, struct limine_framebuffer *fb) {
    for (uint8_t j = 0; j < font.height; j++) {
        for (uint8_t i = 0; i < font.width; i++) {
            uint8_t pixel = ce_font_sample(font, c, i, j);
            // set each component of color to pixel
            uint8_t color = pixel;
            uint32_t fb_x = x + i;
            uint32_t fb_y = y + j;
            write_rgb256_pixel(fb, fb_x, fb_y, (uint8_t[3]){color, color, color});
        }
    }
}

// draw string at x, y
static inline void ce_font_draw_string(ce_font font, const char *str, uint8_t x, uint8_t y, struct limine_framebuffer *fb) {
    for (size_t i = 0; str[i] != '\0'; i++) {
        ce_font_draw_char(font, str[i], x + i * font.width, y, fb);
    }
}

#endif /* CE_FONT_H */
