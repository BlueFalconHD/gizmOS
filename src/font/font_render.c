#include "font_render.h"
#include "font.h"

// UTF-8 Decoder Function
int utf8_decode(const char *s, int *index, uint32_t *codepoint) {
    unsigned char c = (unsigned char)s[*index];
    if (c < 0x80) {
        *codepoint = c;
        (*index)++;
        return 1;
    } else if ((c & 0xE0) == 0xC0) {
        // 2-byte sequence
        if ((s[*index + 1] & 0xC0) != 0x80)
            return -1; // Invalid continuation byte
        *codepoint = ((uint32_t)(c & 0x1F) << 6) |
                     (uint32_t)(s[*index + 1] & 0x3F);
        (*index) += 2;
        return 1;
    } else if ((c & 0xF0) == 0xE0) {
        // 3-byte sequence
        if ((s[*index + 1] & 0xC0) != 0x80 || (s[*index + 2] & 0xC0) != 0x80)
            return -1; // Invalid continuation bytes
        *codepoint = ((uint32_t)(c & 0x0F) << 12) |
                     ((uint32_t)(s[*index + 1] & 0x3F) << 6) |
                     (uint32_t)(s[*index + 2] & 0x3F);
        (*index) += 3;
        return 1;
    } else if ((c & 0xF8) == 0xF0) {
        // 4-byte sequence
        if ((s[*index + 1] & 0xC0) != 0x80 ||
            (s[*index + 2] & 0xC0) != 0x80 ||
            (s[*index + 3] & 0xC0) != 0x80)
            return -1; // Invalid continuation bytes
        *codepoint = ((uint32_t)(c & 0x07) << 18) |
                     ((uint32_t)(s[*index + 1] & 0x3F) << 12) |
                     ((uint32_t)(s[*index + 2] & 0x3F) << 6) |
                     (uint32_t)(s[*index + 3] & 0x3F);
        (*index) += 4;
        return 1;
    } else {
        return -1; // Invalid UTF-8 byte
    }
}

void font_draw_pixel(int c, int x, int y, fb_info *fb) {
    uint8_t pixel[3] = { (uint8_t)(255 * c), (uint8_t)(255 * c), (uint8_t)(255 * c) };
    write_rgb256_pixel(fb, x, y, pixel);
}

int font_print_char(int x, int y, uint32_t codepoint, fb_info *fb) {
    // Use the dimensions from the font
    const int charWidth = FONT_WIDTH;
    const int charHeight = FONT_HEIGHT;
    const int charSpacing = 1; // Adjust spacing as needed

    // Clear the area where the character and spacing will be
    for (int py = 0; py < charHeight; py++) {
        for (int px = 0; px < charWidth + charSpacing; px++) {
            font_draw_pixel(0, x + px, y + py, fb);
        }
    }

    // Special handling for space character
    if (codepoint == ' ') {
        return charWidth + charSpacing;
    }

    // Loop to find the matching character in the font array
    int match = -1;
    int font_size = sizeof(font) / sizeof(font[0]);
    for (int l = 0; l < font_size; l++) {
        if (font[l].codepoint == codepoint) {
            match = l;
            break;
        }
    }

    // If the character is found, render it
    if (match != -1) {
        for (int py = 0; py < charHeight; py++) {
            for (int px = 0; px < charWidth; px++) {
                char pixel_value = font[match].code[py][px];
                if (pixel_value == '#') {
                    font_draw_pixel(1, x + px, y + py, fb);
                } else {
                    // For completeness, you can choose to draw background pixels here
                    // font_draw_pixel(0, x + px, y + py, fb);
                }
            }
        }
    } else {
        // Character not found, you can choose to render a placeholder or skip
        // For now, we'll skip rendering and return the width
        return charWidth + charSpacing;
    }

    // Return the total width consumed by this character including spacing
    return charWidth + charSpacing;
}

int font_print_string(int x, int y, const char *string, fb_info *fb) {
    int cx = x;
    int cy = y;
    int index = 0;

    while (1) {
        if (string[index] == '\0') {
            break;
        }

        uint32_t codepoint;
        int res = utf8_decode(string, &index, &codepoint);
        if (res == -1) {
            // Invalid UTF-8 sequence, skip the byte
            index++;
            continue;
        }

        int length = font_print_char(cx, cy, codepoint, fb);
        cx += length;

        #ifdef SCREEN_WIDTH
        // Text wrap after space or if we reach end of line
        if (cx > SCREEN_WIDTH - FONT_WIDTH && codepoint == ' ') {
            cx = x;
            cy += FONT_HEIGHT; // Move to the next line
        }
        #endif
    }

    return cy;
}
