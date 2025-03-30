#ifndef FB_H
#define FB_H

#include "../limine.h"
#include <stdint.h>

typedef struct limine_framebuffer os_framebuffer_t;

os_framebuffer_t *get_framebuffer(void);

void write_rgb256_pixel(os_framebuffer_t *fb, uint32_t x, uint32_t y,
                        uint8_t pixel[3]);
void draw_rgb256_map(os_framebuffer_t *fb, uint32_t x_res, uint32_t y_res,
                     uint8_t *rgb_map);

#endif /* FB_H */
