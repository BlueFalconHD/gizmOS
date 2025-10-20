#include "ppm.h"

#include <lib/memory.h>

static const uint8_t *skip_ws_and_comments(const uint8_t *p,
                                           const uint8_t *end) {
  while (p < end) {
    /* skip whitespace */
    while (p < end && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n'))
      p++;
    if (p >= end)
      break;
    if (*p == '#') {
      /* skip comment line */
      while (p < end && *p != '\n')
        p++;
      continue;
    }
    break;
  }
  return p;
}

static g_bool parse_uint(const uint8_t **pp, const uint8_t *end,
                         uint32_t *out) {
  const uint8_t *p = skip_ws_and_comments(*pp, end);
  if (p >= end)
    return false;
  uint64_t v = 0;
  g_bool any = false;
  while (p < end && *p >= '0' && *p <= '9') {
    v = v * 10 + (uint64_t)(*p - '0');
    any = true;
    p++;
    if (v > 0xFFFFFFFFull)
      return false;
  }
  if (!any)
    return false;
  *out = (uint32_t)v;
  *pp = p;
  return true;
}

g_bool ppm_parse(const void *data, size_t size, ppm_info_t *out) {
  if (!data || !out || size < 3)
    return false;
  const uint8_t *p = (const uint8_t *)data;
  const uint8_t *end = p + size;

  /* Magic: P6 */
  if (end - p < 2 || p[0] != 'P' || p[1] != '6')
    return false;
  p += 2;

  /* width */
  uint32_t w = 0, h = 0, maxv = 0;
  if (!parse_uint(&p, end, &w))
    return false;
  /* height */
  if (!parse_uint(&p, end, &h))
    return false;
  /* maxval */
  if (!parse_uint(&p, end, &maxv))
    return false;
  if (maxv != 255)
    return false; /* only 8-bit supported */

  /* one whitespace character before binary data */
  p = skip_ws_and_comments(p, end);
  if (p >= end)
    return false;

  size_t needed = (size_t)w * (size_t)h;
  if (needed == 0)
    return false;
  if (needed > (SIZE_MAX / 3))
    return false;
  needed *= 3;
  if ((size_t)(end - p) < needed)
    return false;

  out->width = w;
  out->height = h;
  out->maxval = maxv;
  out->pixel_data = p;
  out->pixel_data_size = needed;
  return true;
}

g_bool ppm_blit_to_framebuffer(framebuffer_t *fb, uint32_t dst_x,
                               uint32_t dst_y, const void *data, size_t size) {
  if (!fb || !fb->framebuffer)
    return false;
  ppm_info_t info;
  if (!ppm_parse(data, size, &info))
    return false;

  uint32_t w = info.width;
  uint32_t h = info.height;
  const uint8_t *pix = info.pixel_data;

  uint8_t rgb[3];
  for (uint32_t y = 0; y < h; ++y) {
    for (uint32_t x = 0; x < w; ++x) {
      size_t off = ((size_t)y * (size_t)w + (size_t)x) * 3;
      rgb[0] = pix[off + 0];
      rgb[1] = pix[off + 1];
      rgb[2] = pix[off + 2];
      framebuffer_put_pixel(fb, dst_x + x, dst_y + y, rgb);
    }
  }
  return true;
}

g_bool ppm_blit_to_surface(PCSurface *surface, int32_t dst_x, int32_t dst_y,
                           const void *data, size_t size) {
  if (!surface || !surface->pixels)
    return false;

  ppm_info_t info;
  if (!ppm_parse(data, size, &info))
    return false;

  int32_t img_w = (int32_t)info.width;
  int32_t img_h = (int32_t)info.height;
  const uint8_t *pix = info.pixel_data;

  /* Compute clipped destination rect within the surface */
  int32_t start_x = dst_x < 0 ? 0 : dst_x;
  int32_t start_y = dst_y < 0 ? 0 : dst_y;
  int32_t end_x = dst_x + img_w;
  int32_t end_y = dst_y + img_h;
  if (end_x > (int32_t)surface->rect.width)
    end_x = (int32_t)surface->rect.width;
  if (end_y > (int32_t)surface->rect.height)
    end_y = (int32_t)surface->rect.height;

  if (end_x <= start_x || end_y <= start_y)
    return true; /* nothing to draw, but not an error */

  /* Write pixels */
  for (int32_t dy = start_y; dy < end_y; ++dy) {
    int32_t sy = dy - dst_y; /* source y */
    uint32_t *dst_row = surface->pixels + (size_t)dy * (size_t)surface->stride;
    for (int32_t dx = start_x; dx < end_x; ++dx) {
      int32_t sx = dx - dst_x; /* source x */
      size_t off = ((size_t)sy * (size_t)info.width + (size_t)sx) * 3;
      uint8_t r = pix[off + 0];
      uint8_t g = pix[off + 1];
      uint8_t b = pix[off + 2];
      /* ARGB 0xAARRGGBB with A=0xFF */
      dst_row[dx] = (0xFFu << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) |
                    (uint32_t)b;
    }
  }

  /* Mark dirty */
  PCRect dirty = {start_x, start_y, (uint32_t)(end_x - start_x),
                  (uint32_t)(end_y - start_y)};
  PCSurface_mark_dirty(surface, dirty);
  return true;
}

PCSurface *ppm_create_surface(const void *data, size_t size, uint32_t z_index) {
  ppm_info_t info;
  if (!ppm_parse(data, size, &info))
    return NULL;

  PCRect r = {0, 0, info.width, info.height};
  PCSurface *s = PCSurface_create(&r, z_index);
  if (!s)
    return NULL;
  if (!ppm_blit_to_surface(s, 0, 0, data, size)) {
    PCSurface_free(s);
    return NULL;
  }
  return s;
}
