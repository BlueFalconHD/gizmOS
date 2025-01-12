#ifndef FONT_RENDER_H
#define FONT_RENDER_H

#include "../device/framebuffer.h"
#include "../limine.h"

#define SCREEN_WIDTH 256

void font_draw_pixel(int c, int x, int y, struct limine_framebuffer *fb);
int font_print_char(int x, int y, uint32_t codepoint, struct limine_framebuffer *fb);
int font_print_string(int x, int y, const char *string, struct limine_framebuffer *fb);

#endif // FONT_RENDER_H
