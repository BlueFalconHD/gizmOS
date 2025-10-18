#include "gizm_font.h"
#include "lib/debug.h"
#include <device/shared.h>
#include <lib/PixelCore/surface.h>
#include <lib/memory.h>
#include <lib/str.h>

// Predefined colors
const gizm_color_t GIZM_COLOR_WHITE = {255, 255, 255};
const gizm_color_t GIZM_COLOR_BLACK = {0, 0, 0};
const gizm_color_t GIZM_COLOR_RED = {255, 0, 0};
const gizm_color_t GIZM_COLOR_GREEN = {0, 255, 0};
const gizm_color_t GIZM_COLOR_BLUE = {0, 0, 255};
const gizm_color_t GIZM_COLOR_YELLOW = {255, 255, 0};
const gizm_color_t GIZM_COLOR_CYAN = {0, 255, 255};
const gizm_color_t GIZM_COLOR_MAGENTA = {255, 0, 255};

// Font glyph data (ASCII 32-126, adapted from blit32)
const gizm_glyph_t gizm_font_glyphs[GIZM_FONT_NUM_GLYPHS] = {
    /* all chars up to 32 are non-printable */
    0x00000000, 0x08021084, 0x0000294a, 0x15f52bea, 0x08fa38be, 0x33a22e60,
    0x2e94d8a6, 0x00001084, 0x10421088, 0x04421082, 0x00a23880, 0x00471000,
    0x04420000, 0x00070000, 0x0c600000, 0x02222200, 0x1d3ad72e, 0x3e4214c4,
    0x3e22222e, 0x1d18320f, 0x210fc888, 0x1d183c3f, 0x1d17844c, 0x0222221f,
    0x1d18ba2e, 0x210f463e, 0x0c6018c0, 0x04401000, 0x10411100, 0x00e03800,
    0x04441040, 0x0802322e, 0x3c1ef62e, 0x231fc544, 0x1f18be2f, 0x3c10862e,
    0x1f18c62f, 0x3e10bc3f, 0x0210bc3f, 0x1d1c843e, 0x2318fe31, 0x3e42109f,
    0x0c94211f, 0x23149d31, 0x3e108421, 0x231ad6bb, 0x239cd671, 0x1d18c62e,
    0x0217c62f, 0x30eac62e, 0x2297c62f, 0x1d141a2e, 0x0842109f, 0x1d18c631,
    0x08454631, 0x375ad631, 0x22a21151, 0x08421151, 0x3e22221f, 0x1842108c,
    0x20820820, 0x0c421086, 0x00004544, 0xbe000000, 0x00000082, 0x1c97b000,
    0x0e949c21, 0x1c10b800, 0x1c94b908, 0x3c1fc5c0, 0x42211c4c, 0x4e87252e,
    0x12949c21, 0x0c210040, 0x8c421004, 0x12519521, 0x0c210842, 0x235aac00,
    0x12949c00, 0x0c949800, 0x4213a526, 0x7087252e, 0x02149800, 0x0e837000,
    0x0c213c42, 0x0e94a400, 0x0464a400, 0x155ac400, 0x36426c00, 0x4e872529,
    0x1e223c00, 0x1843188c, 0x08421084, 0x0c463086, 0x0006d800,
};

void gizm_font_init_context(gizm_font_context_t *ctx, framebuffer_t *fb,
                            gizm_color_t color, uint32_t scale,
                            gizm_font_wrap_t wrap_mode) {
  if (!ctx) {
    dbg("ctx == NULL");
    return;
  }

  ctx->framebuffer = fb;
  ctx->color = color;
  ctx->scale = (scale == 0) ? 1 : scale;
  ctx->wrap_mode = wrap_mode;
}

static g_bool is_printable_char(char c) { return c >= ' ' && c <= '~'; }

uint32_t gizm_font_draw_char(gizm_font_context_t *ctx, uint32_t x, uint32_t y,
                             char c) {
  if (!ctx || !ctx->framebuffer) {
    if (!ctx)
      dbg("ctx == NULL");
    else
      dbg("ctx->framebuffer == NULL");
    return 0;
  }

  // Handle special characters
  switch (c) {
  case ' ':
    return ctx->scale * GIZM_FONT_ADVANCE;
  case '\t':
    return ctx->scale * GIZM_FONT_ADVANCE * 4;
  case '\n':
  case '\r':
    return 0;
  default:
    break;
  }

  // Only render printable characters
  if (!is_printable_char(c))
    return ctx->scale * GIZM_FONT_ADVANCE;

  uint32_t glyph_index = GIZM_FONT_ASCII_TO_INDEX(c);
  if (glyph_index >= GIZM_FONT_NUM_GLYPHS)
    return ctx->scale * GIZM_FONT_ADVANCE;

  gizm_glyph_t glyph = gizm_font_glyphs[glyph_index];
  uint32_t offset_y = y + GIZM_FONT_EXTRA_BITS(glyph) * ctx->scale;

  uint8_t pixel_data[3] = {ctx->color.r, ctx->color.g, ctx->color.b};

  for (uint32_t glyph_y = 0; glyph_y < GIZM_FONT_HEIGHT; glyph_y++) {
    for (uint32_t pixel_y = 0; pixel_y < ctx->scale; pixel_y++) {
      uint32_t draw_y = offset_y + glyph_y * ctx->scale + pixel_y;

      for (uint32_t glyph_x = 0; glyph_x < GIZM_FONT_WIDTH; glyph_x++) {
        uint32_t bit_index = glyph_y * GIZM_FONT_WIDTH + glyph_x;
        uint32_t pixel_set = (glyph >> bit_index) & 1;

        if (pixel_set) {
          for (uint32_t pixel_x = 0; pixel_x < ctx->scale; pixel_x++) {
            uint32_t draw_x = x + glyph_x * ctx->scale + pixel_x;
            framebuffer_put_pixel(ctx->framebuffer, draw_x, draw_y, pixel_data);
          }
        }
      }
    }
  }

  return ctx->scale * GIZM_FONT_ADVANCE;
}

uint32_t gizm_font_draw_string_n(gizm_font_context_t *ctx, uint32_t x,
                                 uint32_t y, const char *str,
                                 uint32_t max_len) {
  if (!ctx || !str) {
    if (!ctx)
      dbg("ctx == NULL");
    else
      dbg("str == NULL");
    return 0;
  }

  uint32_t start_x = x;
  uint32_t current_x = x;
  uint32_t current_y = y;
  uint32_t lines_rendered = 1;
  uint32_t fb_width = ctx->framebuffer->framebuffer->width;
  uint32_t fb_height = ctx->framebuffer->framebuffer->height;

  for (uint32_t i = 0; str[i] && i < max_len; i++) {
    char c = str[i];

    uint32_t char_width = 0;
    uint32_t char_height =
        ctx->scale * (GIZM_FONT_HEIGHT + GIZM_FONT_DESCENDER);

    switch (c) {
    case '\n':
      current_y += ctx->scale * GIZM_FONT_ROW_ADVANCE;
      current_x = start_x;
      lines_rendered++;
      continue;
    case '\r':
      current_x = start_x;
      continue;
    case '\b':
      if (current_x >= ctx->scale * GIZM_FONT_ADVANCE) {
        current_x -= ctx->scale * GIZM_FONT_ADVANCE;
      }
      continue;
    case '\t':
      char_width = ctx->scale * GIZM_FONT_ADVANCE * 4;
      break;
    case ' ':
      char_width = ctx->scale * GIZM_FONT_ADVANCE;
      break;
    default:
      char_width = ctx->scale * GIZM_FONT_ADVANCE;
      break;
    }

    uint32_t end_x = current_x + char_width;
    uint32_t end_y = current_y + char_height;

    if (end_y > fb_height) {
      break;
    }

    if (end_x > fb_width) {
      if (ctx->wrap_mode == GIZM_FONT_WRAP_WRAP) {
        current_y += ctx->scale * GIZM_FONT_ROW_ADVANCE;
        current_x = start_x;
        lines_rendered++;

        end_y = current_y + char_height;
        if (end_y > fb_height) {
          break;
        }
        i--;
        continue;
      } else {
        continue;
      }
    }

    if (current_x >= fb_width) {
      continue;
    }

    if (current_y < fb_height && current_x < fb_width) {
      uint32_t advance = gizm_font_draw_char(ctx, current_x, current_y, c);
      current_x += advance;
    } else {
      current_x += char_width;
    }
  }

  return lines_rendered;
}

uint32_t gizm_font_draw_string(gizm_font_context_t *ctx, uint32_t x, uint32_t y,
                               const char *str) {
  if (!str) {
    dbg("str == NULL");
    return 0;
  }
  return gizm_font_draw_string_n(ctx, x, y, str, strlen(str));
}

uint32_t gizm_font_string_width_n(const char *str, uint32_t max_len,
                                  uint32_t scale) {
  if (!str || scale == 0) {
    if (!str)
      dbg("str == NULL");
    else
      dbg("scale == 0");
    return 0;
  }

  uint32_t width = 0;
  uint32_t current_line_width = 0;
  uint32_t max_width = 0;

  for (uint32_t i = 0; str[i] && i < max_len; i++) {
    char c = str[i];

    switch (c) {
    case '\n':
    case '\r':
      if (current_line_width > max_width) {
        max_width = current_line_width;
      }
      current_line_width = 0;
      break;
    case '\t':
      current_line_width += scale * GIZM_FONT_ADVANCE * 4;
      break;
    case '\b':
      if (current_line_width >= scale * GIZM_FONT_ADVANCE) {
        current_line_width -= scale * GIZM_FONT_ADVANCE;
      }
      break;
    default:
      current_line_width += scale * GIZM_FONT_ADVANCE;
      break;
    }
  }

  if (current_line_width > max_width) {
    max_width = current_line_width;
  }

  return max_width;
}

uint32_t gizm_font_string_width(const char *str, uint32_t scale) {
  if (!str) {
    dbg("str == NULL");
    return 0;
  }
  return gizm_font_string_width_n(str, strlen(str), scale);
}

uint32_t gizm_font_get_height(uint32_t scale) {
  return scale * GIZM_FONT_HEIGHT;
}

uint32_t gizm_font_get_advance(uint32_t scale) {
  return scale * GIZM_FONT_ADVANCE;
}

uint32_t gizm_font_get_row_advance(uint32_t scale) {
  return scale * GIZM_FONT_ROW_ADVANCE;
}

// Convenience functions using shared framebuffer
void gizm_font_draw_text(uint32_t x, uint32_t y, const char *str,
                         gizm_color_t color) {
  if (!shared_framebuffer_initialized || !str) {
    if (!str)
      dbg("str == NULL");
    if (!shared_framebuffer_initialized)
      dbg("shared_framebuffer_initialized == false");
    return;
  }

  gizm_font_context_t ctx;
  gizm_font_init_context(&ctx, shared_framebuffer, color, 1,
                         GIZM_FONT_WRAP_CLIP);
  gizm_font_draw_string(&ctx, x, y, str);
}

void gizm_font_draw_text_scaled(uint32_t x, uint32_t y, const char *str,
                                gizm_color_t color, uint32_t scale) {
  if (!shared_framebuffer_initialized || !str) {
    if (!str)
      dbg("str == NULL");
    if (!shared_framebuffer_initialized)
      dbg("shared_framebuffer_initialized == false");
    return;
  }

  gizm_font_context_t ctx;
  gizm_font_init_context(&ctx, shared_framebuffer, color, scale,
                         GIZM_FONT_WRAP_CLIP);
  gizm_font_draw_string(&ctx, x, y, str);
}

static inline void pc_surface_put_pixel(PCSurface *surface, uint32_t x,
                                        uint32_t y, uint32_t argb) {
  if (!surface) {
    dbg("surface == NULL");
    return;
  }
  if (x >= surface->rect.width || y >= surface->rect.height)
    return;
  surface->pixels[(size_t)y * (size_t)surface->stride + (size_t)x] = argb;
}

static uint32_t gizm_font_draw_char_surface(PCSurface *surface, uint32_t x,
                                            uint32_t y, char c,
                                            gizm_color_t color,
                                            uint32_t scale) {
  if (!surface) {
    dbg("surface == NULL");
    return 0;
  }

  switch (c) {
  case ' ':
    return scale * GIZM_FONT_ADVANCE;
  case '\t':
    return scale * GIZM_FONT_ADVANCE * 4;
  case '\n':
  case '\r':
    return 0;
  default:
    break;
  }

  if (!is_printable_char(c))
    return scale * GIZM_FONT_ADVANCE;

  uint32_t glyph_index = GIZM_FONT_ASCII_TO_INDEX(c);
  if (glyph_index >= GIZM_FONT_NUM_GLYPHS)
    return scale * GIZM_FONT_ADVANCE;

  gizm_glyph_t glyph = gizm_font_glyphs[glyph_index];
  uint32_t offset_y = y + GIZM_FONT_EXTRA_BITS(glyph) * scale;

  uint32_t argb = 0xFF000000u | ((uint32_t)color.r << 16) |
                  ((uint32_t)color.g << 8) | (uint32_t)color.b;

  for (uint32_t gy = 0; gy < GIZM_FONT_HEIGHT; ++gy) {
    for (uint32_t py = 0; py < scale; ++py) {
      uint32_t draw_y = offset_y + gy * scale + py;
      if (draw_y >= surface->rect.height)
        continue;

      for (uint32_t gx = 0; gx < GIZM_FONT_WIDTH; ++gx) {
        uint32_t bit_index = gy * GIZM_FONT_WIDTH + gx;
        uint32_t pixel_set = (glyph >> bit_index) & 1u;

        if (pixel_set) {
          for (uint32_t px = 0; px < scale; ++px) {
            uint32_t draw_x = x + gx * scale + px;
            if (draw_x >= surface->rect.width)
              continue;
            pc_surface_put_pixel(surface, draw_x, draw_y, argb);
          }
        }
      }
    }
  }

  return scale * GIZM_FONT_ADVANCE;
}

void gizm_font_draw_text_surface(PCSurface *surface, uint32_t x, uint32_t y,
                                 const char *str, gizm_color_t color) {
  gizm_font_draw_text_scaled_surface(surface, x, y, str, color, 1);
}

void gizm_font_draw_text_scaled_surface(PCSurface *surface, uint32_t x,
                                        uint32_t y, const char *str,
                                        gizm_color_t color, uint32_t scale) {
  if (!surface || !str || scale == 0) {
    if (!surface)
      dbg("surface == NULL");
    if (!str)
      dbg("str == NULL");
    if (scale == 0)
      dbg("scale == 0");
    return;
  }

  uint32_t start_x = x;
  uint32_t cur_x = x;
  uint32_t cur_y = y;

  uint32_t char_height = scale * (GIZM_FONT_HEIGHT + GIZM_FONT_DESCENDER);
  uint32_t row_advance = gizm_font_get_row_advance(scale);

  uint32_t max_right = cur_x;
  uint32_t bottom = cur_y + char_height;

  for (uint32_t i = 0; str[i]; ++i) {
    char c = str[i];

    if (c == '\n') {
      cur_y += row_advance;
      cur_x = start_x;
      if (cur_y + char_height > bottom)
        bottom = cur_y + char_height;
      continue;
    } else if (c == '\r') {
      cur_x = start_x;
      continue;
    } else if (c == '\b') {
      uint32_t adv = scale * GIZM_FONT_ADVANCE;
      if (cur_x >= adv)
        cur_x -= adv;
      continue;
    } else if (c == '\t') {
      cur_x += scale * GIZM_FONT_ADVANCE * 4;
    } else if (c == ' ') {
      cur_x += scale * GIZM_FONT_ADVANCE;
    } else {
      uint32_t adv =
          gizm_font_draw_char_surface(surface, cur_x, cur_y, c, color, scale);
      cur_x += adv;
    }

    if (cur_x > max_right)
      max_right = cur_x;
  }

  uint32_t width = (max_right > start_x) ? (max_right - start_x) : 0;
  uint32_t height = (bottom > y) ? (bottom - y) : 0;

  if (width > 0 && height > 0) {
    PCRect dirty = {(int32_t)x, (int32_t)y, width, height};
    PCSurface_mark_dirty(surface, dirty);
  }
}
