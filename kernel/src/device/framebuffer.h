#ifndef FB_H
#define FB_H

#include  <stdint.h>
#include  "../limine.h"

struct limine_framebuffer *get_framebuffer(void);

void write_rgb256_pixel(struct limine_framebuffer *fb, uint32_t x, uint32_t y, uint8_t pixel[3]);
void draw_rgb256_map(struct limine_framebuffer *fb, uint32_t x_res, uint32_t y_res, uint8_t *rgb_map);

#endif /* FB_H */
