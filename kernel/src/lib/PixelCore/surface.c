#include "surface.h"

#include "../kalloc.h"
#include "../memory.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static volatile uint32_t g_surface_id_counter = 1;

static inline int32_t i32_max(int32_t a, int32_t b) { return a > b ? a : b; }
static inline int32_t i32_min(int32_t a, int32_t b) { return a < b ? a : b; }

static inline bool rect_is_empty(const PCRect *r) {
  return r->width == 0 || r->height == 0;
}

static inline void rect_union_bounds(const PCRect *a, const PCRect *b,
                                     PCRect *out) {
  int32_t nx = (a->x < b->x) ? a->x : b->x;
  int32_t ny = (a->y < b->y) ? a->y : b->y;

  int64_t ar = (int64_t)a->x + (int64_t)a->width;
  int64_t br = (int64_t)b->x + (int64_t)b->width;
  int64_t ab = (int64_t)a->y + (int64_t)a->height;
  int64_t bb = (int64_t)b->y + (int64_t)b->height;

  int64_t nr = (ar > br) ? ar : br;
  int64_t nb = (ab > bb) ? ab : bb;

  out->x = nx;
  out->y = ny;
  out->width = (uint32_t)(nr - (int64_t)nx);
  out->height = (uint32_t)(nb - (int64_t)ny);
}

static bool rect_clip_to_surface_local(const PCSurface *s, const PCRect *in,
                                       PCRect *out) {
  if (!s || !in || !out)
    return false;

  int32_t x0 = i32_max(0, in->x);
  int32_t y0 = i32_max(0, in->y);
  int64_t in_r = (int64_t)in->x + (int64_t)in->width;
  int64_t in_b = (int64_t)in->y + (int64_t)in->height;
  int32_t x1 = i32_min((int32_t)s->rect.width, (int32_t)in_r);
  int32_t y1 = i32_min((int32_t)s->rect.height, (int32_t)in_b);

  if (x1 <= x0 || y1 <= y0) {
    out->x = 0;
    out->y = 0;
    out->width = 0;
    out->height = 0;
    return false;
  }

  out->x = x0;
  out->y = y0;
  out->width = (uint32_t)(x1 - x0);
  out->height = (uint32_t)(y1 - y0);
  return true;
}

static void surface_dirty_coalesce(PCSurface *surface) {
  if (!surface || surface->dirty_count < 2)
    return;

  bool progress = true;
  while (progress) {
    progress = false;
    for (uint8_t i = 0; i < surface->dirty_count; ++i) {
      for (uint8_t j = (uint8_t)(i + 1); j < surface->dirty_count; ++j) {
        PCRect merged;
        if (PCRect_merge_if_close(&surface->dirty_regions[i],
                                  &surface->dirty_regions[j], &merged)) {
          surface->dirty_regions[i] = merged;
          for (uint8_t k = (uint8_t)(j + 1); k < surface->dirty_count; ++k) {
            surface->dirty_regions[k - 1] = surface->dirty_regions[k];
          }
          surface->dirty_count--;
          progress = true;
          j = i;
        }
      }
    }
  }
}

static void surface_dirty_add(PCSurface *surface, const PCRect *r) {
  if (!surface || !r || rect_is_empty(r))
    return;

  for (uint8_t i = 0; i < surface->dirty_count; ++i) {
    PCRect merged;
    if (PCRect_merge_if_close(&surface->dirty_regions[i], r, &merged)) {
      surface->dirty_regions[i] = merged;
      surface_dirty_coalesce(surface);
      return;
    }
  }

  if (surface->dirty_count < (uint8_t)(sizeof(surface->dirty_regions) /
                                       sizeof(surface->dirty_regions[0]))) {
    surface->dirty_regions[surface->dirty_count++] = *r;
    surface_dirty_coalesce(surface);
    return;
  }

  PCRect bounds = surface->dirty_regions[0];
  for (uint8_t i = 1; i < surface->dirty_count; ++i) {
    rect_union_bounds(&bounds, &surface->dirty_regions[i], &bounds);
  }
  rect_union_bounds(&bounds, r, &bounds);
  surface->dirty_regions[0] = bounds;
  surface->dirty_count = 1;
}

PCSurface *PCSurface_create(PCRect *rect, uint32_t z_index) {
  if (!rect || rect->width == 0 || rect->height == 0)
    return NULL;

  PCSurface *s = (PCSurface *)kalloc(sizeof(PCSurface));
  if (!s)
    return NULL;

  s->rect = *rect;
  s->previous_rect = *rect;
  s->z_index = z_index;
  s->id = __sync_fetch_and_add(&g_surface_id_counter, 1);
  s->stride = rect->width;
  s->backend = PC_SURFACE_BACKEND_CPU;
  s->backend_data = NULL;
  s->flags = PC_SURFACE_VISIBLE;
  s->in_scene = false;
  s->owner = NULL;
  s->dirty_count = 0;

  initlock(&s->lock, "surface");

  size_t pixels_count = (size_t)rect->width * (size_t)rect->height;
  if (pixels_count == 0 || pixels_count > (SIZE_MAX / sizeof(uint32_t))) {
    kfree(s);
    return NULL;
  }

  s->pixels = (uint32_t *)kalloc(pixels_count * sizeof(uint32_t));
  if (!s->pixels) {
    kfree(s);
    return NULL;
  }

  memset(s->pixels, 0, pixels_count * sizeof(uint32_t));
  return s;
}

void PCSurface_free(PCSurface *surface) {
  if (!surface)
    return;

  if (surface->pixels) {
    kfree(surface->pixels);
    surface->pixels = NULL;
  }

  kfree(surface);
}

void PCSurface_mark_dirty(PCSurface *surface, PCRect region) {
  if (!surface)
    return;

  acquire(&surface->lock);

  PCRect clipped;
  if (rect_clip_to_surface_local(surface, &region, &clipped)) {
    surface_dirty_add(surface, &clipped);
  }

  release(&surface->lock);
}

void PCSurface_clear_dirty(PCSurface *surface) {
  if (!surface)
    return;

  acquire(&surface->lock);
  surface->dirty_count = 0;
  release(&surface->lock);
}

bool PCSurface_resize(PCSurface *surface, uint32_t new_width,
                      uint32_t new_height) {
  if (!surface || new_width == 0 || new_height == 0)
    return false;

  acquire(&surface->lock);

  size_t new_count = (size_t)new_width * (size_t)new_height;
  if (new_count == 0 || new_count > (SIZE_MAX / sizeof(uint32_t))) {
    release(&surface->lock);
    return false;
  }

  uint32_t *new_pixels = (uint32_t *)kalloc(new_count * sizeof(uint32_t));
  if (!new_pixels) {
    release(&surface->lock);
    return false;
  }

  memset(new_pixels, 0, new_count * sizeof(uint32_t));

  uint32_t old_w = surface->rect.width;
  uint32_t old_h = surface->rect.height;
  uint32_t copy_w = (old_w < new_width) ? old_w : new_width;
  uint32_t copy_h = (old_h < new_height) ? old_h : new_height;

  if (surface->pixels && copy_w > 0 && copy_h > 0) {
    for (uint32_t y = 0; y < copy_h; ++y) {
      uint32_t *dst = new_pixels + (size_t)y * (size_t)new_width;
      uint32_t *src = surface->pixels + (size_t)y * (size_t)surface->stride;
      memcpy(dst, src, (size_t)copy_w * sizeof(uint32_t));
    }
  }

  if (surface->pixels) {
    kfree(surface->pixels);
  }

  surface->previous_rect = surface->rect;
  surface->pixels = new_pixels;
  surface->rect.width = new_width;
  surface->rect.height = new_height;
  surface->stride = new_width;

  surface->dirty_count = 0;
  PCRect all = {0, 0, new_width, new_height};
  surface_dirty_add(surface, &all);

  release(&surface->lock);
  return true;
}

void PCSurface_reposition(PCSurface *surface, int32_t new_x, int32_t new_y) {
  if (!surface)
    return;

  acquire(&surface->lock);
  surface->previous_rect = surface->rect;
  surface->rect.x = new_x;
  surface->rect.y = new_y;
  release(&surface->lock);
}

void PCSurface_reorder(PCSurface *surface, uint32_t new_z_index) {
  if (!surface)
    return;

  acquire(&surface->lock);
  surface->z_index = new_z_index;
  release(&surface->lock);
}
