#include "font_render.h"
#include "font_l.h"
#include <stdint.h>

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
    uint8_t pixel[3];

    if (c) {
        // Foreground color (e.g., white)
        pixel[0] = 255; // Red
        pixel[1] = 255; // Green
        pixel[2] = 255; // Blue
    } else {
        // Background color (e.g., black)
        pixel[0] = 0;
        pixel[1] = 0;
        pixel[2] = 0;
    }

    write_rgb256_pixel(fb, x, y, pixel);
}


void draw_byte_stride(int x, int y, uint8_t byte, fb_info *fb) {
    for (int i = 0; i < 8; i++) {
        if (byte & (1 << i)) {
            font_draw_pixel(1, x + i, y, fb);
        } else {
            font_draw_pixel(0, x + i, y, fb);
        }
    }
}

int font_print_char(int x, int y, uint32_t codepoint, fb_info *fb) {
    const int charWidth = 8;
    const int charHeight = 16;
    const int charSpacing = 1; // Adjust spacing as needed

    // Check if codepoint is within the valid range
    if (codepoint > 255) {
        // Optionally render a placeholder or skip the character
        return charWidth + charSpacing;
    }

    // Calculate the starting index of the glyph in the font data array
    int glyph_offset = codepoint * charHeight; // Each character has 'charHeight' bytes

    // Loop over each row of the glyph
    for (int py = 0; py < charHeight; py++) {
        uint8_t row_data = FONT_DATA_LARGE[glyph_offset + py];

        // Loop over each pixel in the row
        for (int px = 0; px < charWidth; px++) {
            // Check if the pixel is set (bits are from MSB to LSB)
            int pixel_set = row_data & (1 << (7 - px));

            if (pixel_set) {
                font_draw_pixel(1, x + px, y + py, fb);
            } else {
                // Optionally, draw background pixel
                font_draw_pixel(0, x + px, y + py, fb);
            }
        }
    }

    // Return the total width consumed by this character including spacing
    return charWidth + charSpacing;
}

int font_print_string(int x, int y, const char *string, fb_info *fb) {
    int cx = x;
    int cy = y;
    int index = 0;

    while (string[index] != '\0') {
        uint32_t codepoint;
        int res = utf8_decode(string, &index, &codepoint);

        if (res == -1) {
            // Invalid UTF-8 sequence, skip the byte
            index++;
            continue;
        }

        // Render the character and get the width
        int length = font_print_char(cx, cy, codepoint, fb);
        cx += length;

        // Handle line wrapping if needed
        // Assuming SCREEN_WIDTH is defined and represents the width of the framebuffer
        #ifdef SCREEN_WIDTH
        if (cx > SCREEN_WIDTH - 8) { // Adjust '8' based on charWidth
            cx = x;
            cy += 8; // Move to next line; adjust based on charHeight
        }
        #endif
    }

    return cy;
}
