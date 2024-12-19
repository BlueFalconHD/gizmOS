#include "font.h"
#include "font_render.h"
#include "../device/framebuffer.h"

void font_draw_pixel(int c, int x, int y, fb_info *fb) {
    uint8_t pixel[3] = {255 * c, 255 * c, 255 * c};
    write_rgb256_pixel(fb, x, y, pixel);
}

int font_print_char(int x, int y, char c, fb_info *fb) {
    const int charWidth = 5;
    const int charHeight = 7;
    const int spacing = 3;

    // First, clear the area where the character and spacing will be
    for (int py = 0; py < charHeight; py++) {
        for (int px = 0; px < charWidth + spacing; px++) {
            font_draw_pixel(0, x + px, y + py, fb);
        }
    }

    // If the character is a space, just return the width including spacing
    if (c == ' ') {
        return charWidth + spacing;
    }

    // Loop to find the matching character in the font array
    int match = -1;
    for (int l = 0; font[l].letter != 0; l++) {
        if (font[l].letter == c) {
            match = l;
            break;
        }
    }

    // If the character is found, render it
    if (match != -1) {
        for (int py = 0; py < charHeight; py++) {
            for (int px = 0; px < charWidth; px++) {
                if (font[match].code[py][px] == '#') {
                    font_draw_pixel(1, x + px, y + py, fb);
                }
            }
        }
    }

    // Return the total width consumed by this character including spacing
    return charWidth + spacing;
}

int font_print_string(int x, int y, char *string, fb_info *fb) {
    int cx = x;
    int cy = y;

    for (int c = 0; string[c] != '\0'; c++) {
        int length = font_print_char(cx, cy, string[c], fb);
        cx += length;

        #ifdef SCREEN_WIDTH
        // Text wrap after space
        if (cx > SCREEN_WIDTH - 20 && string[c] == ' ') {
            cx = x;
            cy += 8; // Move to the next line
        }
        #endif
    }

    return cy;
}
