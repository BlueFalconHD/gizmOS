#ifndef FONT_RENDER_H
#define FONT_RENDER_H

#include "../device/framebuffer.h"

#define SCREEN_WIDTH 256

void font_draw_pixel(int c, int x, int y, fb_info *fb);
int font_print_string(int x, int y, char *string, fb_info *fb);
int font_print_char(int x, int y, char c, fb_info *fb);

#endif // FONT_RENDER_H
