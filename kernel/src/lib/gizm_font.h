#pragma once

#include <device/framebuffer.h>
#include <lib/types.h>
#include <stdint.h>

#define GIZM_FONT_WIDTH 5
#define GIZM_FONT_HEIGHT 6
#define GIZM_FONT_ADVANCE (GIZM_FONT_WIDTH + 1)
#define GIZM_FONT_DESCENDER 2
#define GIZM_FONT_BASELINE_OFFSET (GIZM_FONT_DESCENDER + 1)
#define GIZM_FONT_ROW_ADVANCE (GIZM_FONT_HEIGHT + GIZM_FONT_BASELINE_OFFSET)
#define GIZM_FONT_NUM_GLYPHS 95
#define GIZM_FONT_EXTRA_BITS(x) (((x) >> 30) & 3)

typedef uint32_t gizm_glyph_t;

typedef struct {
  uint8_t r;
  uint8_t g;
  uint8_t b;
} gizm_color_t;

typedef enum { GIZM_FONT_WRAP_CLIP, GIZM_FONT_WRAP_WRAP } gizm_font_wrap_t;

typedef struct {
  framebuffer_t *framebuffer;
  gizm_color_t color;
  uint32_t scale;
  gizm_font_wrap_t wrap_mode;
} gizm_font_context_t;

// Predefined colors
extern const gizm_color_t GIZM_COLOR_WHITE;
extern const gizm_color_t GIZM_COLOR_BLACK;
extern const gizm_color_t GIZM_COLOR_RED;
extern const gizm_color_t GIZM_COLOR_GREEN;
extern const gizm_color_t GIZM_COLOR_BLUE;
extern const gizm_color_t GIZM_COLOR_YELLOW;
extern const gizm_color_t GIZM_COLOR_CYAN;
extern const gizm_color_t GIZM_COLOR_MAGENTA;

// Font glyph data (covers ASCII 32-126)
extern const gizm_glyph_t gizm_font_glyphs[GIZM_FONT_NUM_GLYPHS];

// Convert ASCII character to glyph index
#define GIZM_FONT_ASCII_TO_INDEX(ascii) ((ascii) - ' ')
#define GIZM_FONT_INDEX_TO_ASCII(index) ((index) + ' ')

// Helper macros for color creation
#define GIZM_COLOR(r, g, b) ((gizm_color_t){(r), (g), (b)})
#define GIZM_COLOR_RGB(rgb)                                                    \
  ((gizm_color_t){((rgb) >> 16) & 0xFF, ((rgb) >> 8) & 0xFF, (rgb) & 0xFF})

/**
 * Initialize a font rendering context
 * @param ctx Context to initialize
 * @param fb Framebuffer to render to
 * @param color Text color
 * @param scale Text scale (1 = normal size, 2 = double size, etc.)
 * @param wrap_mode How to handle text that goes off screen
 */
void gizm_font_init_context(gizm_font_context_t *ctx, framebuffer_t *fb,
                            gizm_color_t color, uint32_t scale,
                            gizm_font_wrap_t wrap_mode);

/**
 * Render a single character at the specified position
 * @param ctx Font rendering context
 * @param x X coordinate (top-left of character)
 * @param y Y coordinate (top-left of character)
 * @param c Character to render
 * @return Width of the rendered character in pixels
 */
uint32_t gizm_font_draw_char(gizm_font_context_t *ctx, uint32_t x, uint32_t y,
                             char c);

/**
 * Render a string at the specified position
 * @param ctx Font rendering context
 * @param x X coordinate (top-left of first character)
 * @param y Y coordinate (top-left of first character)
 * @param str String to render
 * @return Number of lines rendered
 */
uint32_t gizm_font_draw_string(gizm_font_context_t *ctx, uint32_t x, uint32_t y,
                               const char *str);

/**
 * Render a string with length limit at the specified position
 * @param ctx Font rendering context
 * @param x X coordinate (top-left of first character)
 * @param y Y coordinate (top-left of first character)
 * @param str String to render
 * @param max_len Maximum number of characters to render
 * @return Number of lines rendered
 */
uint32_t gizm_font_draw_string_n(gizm_font_context_t *ctx, uint32_t x,
                                 uint32_t y, const char *str, uint32_t max_len);

/**
 * Calculate the width of a string in pixels
 * @param str String to measure
 * @param scale Text scale
 * @return Width in pixels
 */
uint32_t gizm_font_string_width(const char *str, uint32_t scale);

/**
 * Calculate the width of a string with length limit in pixels
 * @param str String to measure
 * @param max_len Maximum number of characters to measure
 * @param scale Text scale
 * @return Width in pixels
 */
uint32_t gizm_font_string_width_n(const char *str, uint32_t max_len,
                                  uint32_t scale);

/**
 * Get the height of text at a given scale
 * @param scale Text scale
 * @return Height in pixels
 */
uint32_t gizm_font_get_height(uint32_t scale);

/**
 * Get the advance width (spacing) for a character at a given scale
 * @param scale Text scale
 * @return Advance width in pixels
 */
uint32_t gizm_font_get_advance(uint32_t scale);

/**
 * Get the row advance (line spacing) at a given scale
 * @param scale Text scale
 * @return Row advance in pixels
 */
uint32_t gizm_font_get_row_advance(uint32_t scale);

// Convenience functions that work with shared framebuffer
/**
 * Draw text on the shared framebuffer with default settings
 * @param x X coordinate
 * @param y Y coordinate
 * @param str String to draw
 * @param color Text color
 */
void gizm_font_draw_text(uint32_t x, uint32_t y, const char *str,
                         gizm_color_t color);

/**
 * Draw scaled text on the shared framebuffer
 * @param x X coordinate
 * @param y Y coordinate
 * @param str String to draw
 * @param color Text color
 * @param scale Text scale
 */
void gizm_font_draw_text_scaled(uint32_t x, uint32_t y, const char *str,
                                gizm_color_t color, uint32_t scale);
