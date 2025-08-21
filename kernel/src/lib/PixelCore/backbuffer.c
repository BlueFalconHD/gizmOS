#include "backbuffer.h"

#include "../kalloc.h"
#include "../memory.h"
#include "../spinlock.h"
#include <device/framebuffer.h>
#include <stddef.h>
#include <stdint.h>

// #define PIXELCORE_DRAW_DEBUG

static inline int32_t i32_min(int32_t a, int32_t b) { return a < b ? a : b; }
static inline int32_t i32_max(int32_t a, int32_t b) { return a > b ? a : b; }

static inline void bb_screen_rect(PCBackBuffer *bb, PCRect *out) {
  out->x = 0;
  out->y = 0;
  out->width = bb->width;
  out->height = bb->height;
}

static inline g_bool rect_clip(const PCRect *a, const PCRect *b, PCRect *out) {
  return PCRect_intersection(a, b, out);
}

#ifdef PIXELCORE_DRAW_DEBUG
static inline void debug_putpixel(PCBackBuffer *bb, int32_t x, int32_t y,
                                  uint32_t color) {
  if (x < 0 || y < 0)
    return;
  if ((uint32_t)x >= bb->width || (uint32_t)y >= bb->height)
    return;
  bb->pixels[(size_t)y * (size_t)bb->stride + (size_t)x] = color;
}

static void debug_draw_rect_outline(PCBackBuffer *bb, const PCRect *r,
                                    uint32_t color) {
  if (!bb || !r)
    return;

  int32_t x0 = i32_max(0, r->x);
  int32_t y0 = i32_max(0, r->y);
  int32_t x1 = i32_min((int32_t)bb->width - 1, r->x + (int32_t)r->width - 1);
  int32_t y1 = i32_min((int32_t)bb->height - 1, r->y + (int32_t)r->height - 1);

  if (x1 < x0 || y1 < y0)
    return;

  for (int32_t x = x0; x <= x1; ++x) {
    debug_putpixel(bb, x, y0, color);
    debug_putpixel(bb, x, y1, color);
  }
  for (int32_t y = y0; y <= y1; ++y) {
    debug_putpixel(bb, x0, y, color);
    debug_putpixel(bb, x1, y, color);
  }
}
#endif

static void sort_surfaces(PCBackBuffer *bb) {
  if (!bb || bb->surface_count < 2)
    return;
  for (uint16_t i = 1; i < bb->surface_count; ++i) {
    PCSurface *key = bb->surfaces[i];
    int16_t j = (int16_t)i - 1;
    while (j >= 0) {
      PCSurface *s = bb->surfaces[j];
      if (s->z_index < key->z_index)
        break;
      if (s->z_index == key->z_index && s->id <= key->id)
        break;
      bb->surfaces[j + 1] = s;
      --j;
    }
    bb->surfaces[j + 1] = key;
  }
}

static void add_damage_clip_bb(PCBackBuffer *bb, const PCRect *r) {
  PCRect scr, clipped;
  bb_screen_rect(bb, &scr);
  if (rect_clip(r, &scr, &clipped)) {
    PCRegion_add_coalescing(&bb->collect, &clipped);
  }
}

static void blit_surface_rect_to_bb(PCBackBuffer *bb, PCSurface *s,
                                    const PCRect *part) {
  if (!bb || !s || !part || part->width == 0 || part->height == 0)
    return;
  int32_t src_x = part->x - s->rect.x;
  int32_t src_y = part->y - s->rect.y;
  if (src_x < 0 || src_y < 0)
    return;

  if (s->flags & PC_SURFACE_OPAQUE) {
    for (uint32_t row = 0; row < part->height; ++row) {
      uint32_t *src =
          s->pixels + (size_t)(src_y + row) * (size_t)s->stride + (size_t)src_x;
      uint32_t *dst = bb->pixels +
                      (size_t)(part->y + row) * (size_t)bb->stride +
                      (size_t)part->x;
      memcpy(dst, src, (size_t)part->width * sizeof(uint32_t));
    }
  } else {
    for (uint32_t row = 0; row < part->height; ++row) {
      uint32_t *src_line =
          s->pixels + (size_t)(src_y + row) * (size_t)s->stride + (size_t)src_x;
      uint32_t *dst_line = bb->pixels +
                           (size_t)(part->y + row) * (size_t)bb->stride +
                           (size_t)part->x;
      for (uint32_t col = 0; col < part->width; ++col) {
        uint32_t sp = src_line[col];
        uint32_t dp = dst_line[col];
        uint32_t sa = (sp >> 24) & 0xFFu;
        if (sa == 0) {
          continue;
        } else if (sa == 255) {
          dst_line[col] = sp;
          continue;
        }
        uint32_t sr = (sp >> 16) & 0xFFu;
        uint32_t sg = (sp >> 8) & 0xFFu;
        uint32_t sb = sp & 0xFFu;
        uint32_t dr = (dp >> 16) & 0xFFu;
        uint32_t dg = (dp >> 8) & 0xFFu;
        uint32_t db = dp & 0xFFu;
        uint32_t inv = 255u - sa;
        uint32_t rr = (sr * sa + dr * inv) / 255u;
        uint32_t rg = (sg * sa + dg * inv) / 255u;
        uint32_t rb = (sb * sa + db * inv) / 255u;
        dst_line[col] = (0xFFu << 24) | (rr << 16) | (rg << 8) | rb;
      }
    }
  }
}

PCBackBuffer *PCBackBuffer_create(uint32_t width, uint32_t height) {
  if (width == 0 || height == 0)
    return NULL;

  PCBackBuffer *bb = (PCBackBuffer *)kalloc(sizeof(PCBackBuffer));
  if (!bb)
    return NULL;

  bb->width = width;
  bb->height = height;
  bb->stride = width;
  bb->surface_count = 0;
  bb->frame_counter = 0;

  size_t px_count = (size_t)width * (size_t)height;
  if (px_count == 0 || px_count > (SIZE_MAX / sizeof(uint32_t))) {
    kfree(bb);
    return NULL;
  }

  bb->pixels = (uint32_t *)kalloc(px_count * sizeof(uint32_t));
  if (!bb->pixels) {
    kfree(bb);
    return NULL;
  }
  memset(bb->pixels, 0, px_count * sizeof(uint32_t));

  PCRegion_init(&bb->dirty, bb->dirty_storage, PC_BACKBUFFER_MAX_DIRTY);
  PCRegion_init(&bb->collect, bb->collect_storage, PC_BACKBUFFER_MAX_DIRTY);
  PCRegion_init(&bb->written, bb->written_storage, PC_BACKBUFFER_MAX_DIRTY);

  initlock(&bb->lock, "backbuffer");
  return bb;
}

void PCBackBuffer_free(PCBackBuffer *bb) {
  if (!bb)
    return;
  if (bb->pixels) {
    kfree(bb->pixels);
    bb->pixels = NULL;
  }
  kfree(bb);
}

g_bool PCBackBuffer_add_surface(PCBackBuffer *bb, PCSurface *s) {
  if (!bb || !s)
    return false;

  acquire(&bb->lock);

  if (bb->surface_count >= PC_BACKBUFFER_MAX_SURFACES) {
    release(&bb->lock);
    return false;
  }

  bb->surfaces[bb->surface_count++] = s;
  s->owner = bb;
  s->in_scene = true;

  if (s->flags & PC_SURFACE_VISIBLE) {
    add_damage_clip_bb(bb, &s->rect);
  }

  sort_surfaces(bb);

  release(&bb->lock);
  return true;
}

void PCBackBuffer_remove_surface(PCBackBuffer *bb, PCSurface *s) {
  if (!bb || !s)
    return;

  acquire(&bb->lock);

  int idx = -1;
  for (uint16_t i = 0; i < bb->surface_count; ++i) {
    if (bb->surfaces[i] == s) {
      idx = (int)i;
      break;
    }
  }
  if (idx >= 0) {
    for (uint16_t j = (uint16_t)idx + 1; j < bb->surface_count; ++j) {
      bb->surfaces[j - 1] = bb->surfaces[j];
    }
    bb->surface_count--;
  }

  acquire(&s->lock);
  PCRect old = s->previous_rect;
  PCRect now = s->rect;
  s->owner = NULL;
  s->in_scene = false;
  release(&s->lock);

  add_damage_clip_bb(bb, &old);
  add_damage_clip_bb(bb, &now);

  release(&bb->lock);
}

void PCBackBuffer_on_surface_reordered(PCBackBuffer *bb, PCSurface *s) {
  if (!bb || !s)
    return;
  acquire(&bb->lock);
  sort_surfaces(bb);
  add_damage_clip_bb(bb, &s->rect);
  release(&bb->lock);
}

void PCBackBuffer_on_surface_moved(PCBackBuffer *bb, PCSurface *s) {
  if (!bb || !s)
    return;
  acquire(&bb->lock);
  acquire(&s->lock);
  PCRect old = s->previous_rect;
  PCRect now = s->rect;
  release(&s->lock);
  add_damage_clip_bb(bb, &old);
  add_damage_clip_bb(bb, &now);
  release(&bb->lock);
}

void PCBackBuffer_on_surface_resized(PCBackBuffer *bb, PCSurface *s) {
  if (!bb || !s)
    return;
  acquire(&bb->lock);
  acquire(&s->lock);
  PCRect old = s->previous_rect;
  PCRect now = s->rect;
  release(&s->lock);
  add_damage_clip_bb(bb, &old);
  add_damage_clip_bb(bb, &now);
  release(&bb->lock);
}

void PCBackBuffer_compose(PCBackBuffer *bb) {
  if (!bb)
    return;

  acquire(&bb->lock);

  PCRect screen;
  bb_screen_rect(bb, &screen);

  PCRect work_store[PC_BACKBUFFER_MAX_DIRTY];
  PCRegion work;
  PCRegion_init(&work, work_store, PC_BACKBUFFER_MAX_DIRTY);

  if (bb->collect.all) {
    PCRegion_add_coalescing(&work, &screen);
  } else {
    for (uint16_t i = 0; i < bb->collect.count; ++i) {
      PCRect clipped;
      if (rect_clip(&bb->collect.r[i], &screen, &clipped)) {
        PCRegion_add_coalescing(&work, &clipped);
      }
    }
  }

  for (uint16_t i = 0; i < bb->surface_count; ++i) {
    PCSurface *s = bb->surfaces[i];
    if (!(s->flags & PC_SURFACE_VISIBLE))
      continue;
    acquire(&s->lock);
    if (!(s->flags & PC_SURFACE_VISIBLE)) {
      release(&s->lock);
      continue;
    }
    for (uint8_t d = 0; d < s->dirty_count; ++d) {
      PCRect g = s->dirty_regions[d];
      g.x += s->rect.x;
      g.y += s->rect.y;
      PCRect clipped;
      if (rect_clip(&g, &screen, &clipped)) {
        PCRegion_add_coalescing(&work, &clipped);
      }
    }
    release(&s->lock);
  }

  PCRegion_clear(&bb->written);

  for (uint16_t i = 0; i < bb->surface_count; ++i) {
    PCSurface *s = bb->surfaces[i];
    if (!(s->flags & PC_SURFACE_VISIBLE)) {
      continue;
    }

    PCRect vis_store[PC_BACKBUFFER_MAX_DIRTY];
    PCRegion vis;
    PCRegion_init(&vis, vis_store, PC_BACKBUFFER_MAX_DIRTY);
    PCRegion_intersect_clip(&work, &s->rect, &vis);
    if (vis.count == 0 && !vis.all)
      continue;

    PCRect higher_store[PC_BACKBUFFER_MAX_DIRTY];
    PCRegion higher;
    PCRegion_init(&higher, higher_store, PC_BACKBUFFER_MAX_DIRTY);

    for (int j = (int)bb->surface_count - 1; j > (int)i; --j) {
      PCSurface *sj = bb->surfaces[j];
      if (!(sj->flags & PC_SURFACE_VISIBLE))
        continue;
      if (!(sj->flags & PC_SURFACE_OPAQUE))
        continue;
      PCRect tmp_store[PC_BACKBUFFER_MAX_DIRTY];
      PCRegion tmp;
      PCRegion_init(&tmp, tmp_store, PC_BACKBUFFER_MAX_DIRTY);
      PCRegion_intersect_clip(&vis, &sj->rect, &tmp);
      for (uint16_t tr = 0; tr < tmp.count; ++tr) {
        PCRegion_add_no_merge(&higher, &tmp.r[tr]);
      }
    }

    for (uint16_t vr = 0; vr < vis.count; ++vr) {
      PCRect pieces[PC_BACKBUFFER_MAX_DIRTY];
      uint16_t pn = PCRect_subtract_by_region(&vis.r[vr], &higher, pieces,
                                              PC_BACKBUFFER_MAX_DIRTY);
      for (uint16_t p = 0; p < pn; ++p) {
        blit_surface_rect_to_bb(bb, s, &pieces[p]);
        PCRegion_add_coalescing(&bb->written, &pieces[p]);
      }
    }
  }

#ifdef PIXELCORE_DRAW_DEBUG
  {
    uint32_t color_collect = 0xFF0072B2u;
    uint32_t color_work = 0xFFF0E442u;
    uint32_t color_written = 0xFFD55E00u;

    if (bb->collect.all) {
      PCRect scr_dbg;
      bb_screen_rect(bb, &scr_dbg);
      debug_draw_rect_outline(bb, &scr_dbg, color_collect);
      PCRegion_add_coalescing(&bb->written, &scr_dbg);
    } else {
      for (uint16_t i = 0; i < bb->collect.count; ++i) {
        debug_draw_rect_outline(bb, &bb->collect.r[i], color_collect);
        PCRegion_add_coalescing(&bb->written, &bb->collect.r[i]);
      }
    }

    for (uint16_t i = 0; i < work.count; ++i) {
      debug_draw_rect_outline(bb, &work.r[i], color_work);
      PCRegion_add_coalescing(&bb->written, &work.r[i]);
    }

    for (uint16_t i = 0; i < bb->written.count; ++i) {
      debug_draw_rect_outline(bb, &bb->written.r[i], color_written);
    }

    for (uint16_t si = 0; si < bb->surface_count; ++si) {
      PCSurface *ss = bb->surfaces[si];
      if (!(ss->flags & PC_SURFACE_VISIBLE))
        continue;
      uint32_t col = 0xFFCC79A7u;
      debug_draw_rect_outline(bb, &ss->rect, col);
      PCRegion_add_coalescing(&bb->written, &ss->rect);
    }
  }
#endif

  PCRegion_clear(&bb->dirty);

  for (uint16_t i = 0; i < bb->written.count; ++i) {
    PCRegion_add_coalescing(&bb->dirty, &bb->written.r[i]);
  }

  for (uint16_t i = 0; i < bb->surface_count; ++i) {
    PCSurface *s = bb->surfaces[i];
    acquire(&s->lock);
    s->dirty_count = 0;
    s->previous_rect = s->rect;
    release(&s->lock);
  }

  PCRegion_clear(&bb->collect);

  bb->frame_counter++;

  release(&bb->lock);
}

void PCBackBuffer_flush(PCBackBuffer *bb, uint32_t *dst_pixels,
                        uint32_t dst_width, uint32_t dst_height,
                        uint32_t dst_stride_pixels) {
  if (!bb || !dst_pixels)

    return;

  acquire(&bb->lock);

  uint32_t fbw = dst_width;
  uint32_t fbh = dst_height;

  for (uint16_t i = 0; i < bb->dirty.count; ++i) {
    PCRect r = bb->dirty.r[i];

    int32_t x0 = i32_max(0, r.x);
    int32_t y0 = i32_max(0, r.y);
    int32_t x1 = i32_min((int32_t)fbw, r.x + (int32_t)r.width);
    int32_t y1 = i32_min((int32_t)fbh, r.y + (int32_t)r.height);
    if (x1 <= x0 || y1 <= y0)
      continue;

    uint32_t w = (uint32_t)(x1 - x0);
    uint32_t h = (uint32_t)(y1 - y0);

    for (uint32_t row = 0; row < h; ++row) {
      uint32_t *dst = dst_pixels +
                      (size_t)(y0 + row) * (size_t)dst_stride_pixels +
                      (size_t)x0;
      uint32_t *src =
          bb->pixels + (size_t)(y0 + row) * (size_t)bb->stride + (size_t)x0;
      memcpy(dst, src, (size_t)w * sizeof(uint32_t));
    }
  }

  PCRegion_clear(&bb->dirty);

  release(&bb->lock);
}

void PCBackBuffer_flush_to_framebuffer(PCBackBuffer *bb, framebuffer_t *fb) {
  if (!bb || !fb)
    return;

  PCBackBuffer_flush(bb, fb->framebuffer->address, fb->framebuffer->width,
                     fb->framebuffer->height,
                     fb->framebuffer->pitch / (fb->framebuffer->bpp / 8));
}
