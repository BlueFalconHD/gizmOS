#include "region.h"

#include <stdbool.h>
#include <stdint.h>

static inline bool rect_is_empty(const PCRect *r) {
  return r->width == 0 || r->height == 0;
}

static inline int64_t rect_right(const PCRect *r) {
  return (int64_t)r->x + (int64_t)r->width;
}

static inline int64_t rect_bottom(const PCRect *r) {
  return (int64_t)r->y + (int64_t)r->height;
}

static inline int32_t min_i32(int32_t a, int32_t b) { return a < b ? a : b; }
static inline int32_t max_i32(int32_t a, int32_t b) { return a > b ? a : b; }
static inline int64_t min_i64(int64_t a, int64_t b) { return a < b ? a : b; }
static inline int64_t max_i64(int64_t a, int64_t b) { return a > b ? a : b; }

static void rect_union_bounds(const PCRect *a, const PCRect *b, PCRect *out) {
  int32_t nx = min_i32(a->x, b->x);
  int32_t ny = min_i32(a->y, b->y);
  int64_t ar = rect_right(a);
  int64_t br = rect_right(b);
  int64_t ab = rect_bottom(a);
  int64_t bb = rect_bottom(b);
  int64_t nr = max_i64(ar, br);
  int64_t nb = max_i64(ab, bb);

  out->x = nx;
  out->y = ny;
  out->width = (uint32_t)(nr - (int64_t)nx);
  out->height = (uint32_t)(nb - (int64_t)ny);
}

// Returns true if a and b overlap or touch along an edge, and writes the
// bounding union into out. Corner-only touching does not count.
static bool rect_merge_if_touch_or_overlap(const PCRect *a, const PCRect *b,
                                           PCRect *out) {
  int64_t ax1 = rect_right(a);
  int64_t ay1 = rect_bottom(a);
  int64_t bx1 = rect_right(b);
  int64_t by1 = rect_bottom(b);

  bool overlap = ((int64_t)a->x < bx1) && ((int64_t)b->x < ax1) &&
                 ((int64_t)a->y < by1) && ((int64_t)b->y < ay1);

  bool x_adjacent = (ax1 == (int64_t)b->x || bx1 == (int64_t)a->x) &&
                    ((int64_t)a->y < by1) && ((int64_t)b->y < ay1);

  bool y_adjacent = (ay1 == (int64_t)b->y || by1 == (int64_t)a->y) &&
                    ((int64_t)a->x < bx1) && ((int64_t)b->x < ax1);

  if (overlap || x_adjacent || y_adjacent) {
    rect_union_bounds(a, b, out);
    return true;
  }
  return false;
}

static void region_compact(PCRegion *rg) {
  if (!rg || rg->count < 2)
    return;

  bool progress = true;
  while (progress) {
    progress = false;
    for (uint16_t i = 0; i < rg->count; ++i) {
      for (uint16_t j = (uint16_t)(i + 1); j < rg->count; ++j) {
        PCRect merged;
        if (rect_merge_if_touch_or_overlap(&rg->r[i], &rg->r[j], &merged)) {
          rg->r[i] = merged;
          // Remove j by shifting left
          for (uint16_t k = j + 1; k < rg->count; ++k) {
            rg->r[k - 1] = rg->r[k];
          }
          rg->count--;
          progress = true;
          j = i;
        }
      }
    }
  }
}

void PCRegion_init(PCRegion *rg, PCRect *storage, uint16_t capacity) {
  if (!rg)
    return;
  rg->r = storage;
  rg->count = 0;
  rg->capacity = capacity;
  rg->all = false;
}

void PCRegion_clear(PCRegion *rg) {
  if (!rg)
    return;
  rg->count = 0;
  rg->all = false;
}

void PCRegion_add_coalescing(PCRegion *rg, const PCRect *rect) {
  if (!rg || !rect)
    return;
  if (rg->capacity == 0)
    return;
  if (rect_is_empty(rect))
    return;

  PCRect candidate = *rect;

  bool merged = true;
  while (merged) {
    merged = false;
    for (uint16_t i = 0; i < rg->count; ++i) {
      PCRect m;
      if (rect_merge_if_touch_or_overlap(&candidate, &rg->r[i], &m)) {
        candidate = m;
        // remove i
        for (uint16_t k = i + 1; k < rg->count; ++k) {
          rg->r[k - 1] = rg->r[k];
        }
        rg->count--;
        merged = true;
        i = (uint16_t)-1;
      }
    }
  }

  if (rg->count < rg->capacity) {
    rg->r[rg->count++] = candidate;
    return;
  }

  region_compact(rg);

  merged = true;
  while (merged) {
    merged = false;
    for (uint16_t i = 0; i < rg->count; ++i) {
      PCRect m;
      if (rect_merge_if_touch_or_overlap(&candidate, &rg->r[i], &m)) {
        candidate = m;
        for (uint16_t k = i + 1; k < rg->count; ++k) {
          rg->r[k - 1] = rg->r[k];
        }
        rg->count--;
        merged = true;
        i = (uint16_t)-1;
      }
    }
  }

  if (rg->count < rg->capacity) {
    rg->r[rg->count++] = candidate;
    return;
  }

  PCRect bounds = candidate;
  for (uint16_t i = 0; i < rg->count; ++i) {
    rect_union_bounds(&bounds, &rg->r[i], &bounds);
  }
  rg->r[0] = bounds;
  rg->count = 1;
}

void PCRegion_add_no_merge(PCRegion *rg, const PCRect *rect) {
  if (!rg || !rect)
    return;
  if (rg->capacity == 0)
    return;
  if (rect_is_empty(rect))
    return;

  if (rg->count < rg->capacity) {
    rg->r[rg->count++] = *rect;
  } else {
  }
}

void PCRegion_translate(const PCRegion *in, PCRegion *out, int32_t dx,
                        int32_t dy) {
  if (!in || !out)
    return;

  bool in_all = in->all;

  PCRegion_clear(out);
  out->all = in_all;

  if (in_all) {
    return;
  }

  for (uint16_t i = 0; i < in->count; ++i) {
    PCRect tr;
    tr.x = in->r[i].x + dx;
    tr.y = in->r[i].y + dy;
    tr.width = in->r[i].width;
    tr.height = in->r[i].height;
    PCRegion_add_coalescing(out, &tr);
  }
}

void PCRegion_intersect_clip(const PCRegion *in, const PCRect *clip,
                             PCRegion *out) {
  if (!in || !clip || !out)
    return;

  PCRegion_clear(out);

  if (rect_is_empty(clip)) {
    return;
  }

  if (in->all) {
    PCRegion_add_coalescing(out, clip);
    return;
  }

  for (uint16_t i = 0; i < in->count; ++i) {
    PCRect inter;
    if (PCRect_intersection(&in->r[i], clip, &inter)) {
      PCRegion_add_coalescing(out, &inter);
    }
  }
}

uint16_t PCRect_subtract_by_rect(const PCRect *src, const PCRect *cut,
                                 PCRect out[], uint16_t out_cap) {
  if (!src || out_cap == 0)
    return 0;

  if (rect_is_empty(src)) {
    return 0;
  }

  if (!cut || rect_is_empty(cut)) {
    out[0] = *src;
    return 1;
  }

  PCRect isect;
  if (!PCRect_intersection(src, cut, &isect)) {
    out[0] = *src;
    return 1;
  }

  uint16_t n = 0;

  int32_t sx = src->x;
  int32_t sy = src->y;
  int32_t sr = (int32_t)src->x + (int32_t)src->width;
  int32_t sb = (int32_t)src->y + (int32_t)src->height;

  int32_t ix = isect.x;
  int32_t iy = isect.y;
  int32_t ir = (int32_t)isect.x + (int32_t)isect.width;
  int32_t ib = (int32_t)isect.y + (int32_t)isect.height;

  if (iy > sy) {
    PCRect top = {sx, sy, src->width, (uint32_t)(iy - sy)};
    if (top.width > 0 && top.height > 0) {
      if (n < out_cap)
        out[n++] = top;
    }
  }

  if (ib < sb) {
    PCRect bottom = {sx, ib, src->width, (uint32_t)(sb - ib)};
    if (bottom.width > 0 && bottom.height > 0) {
      if (n < out_cap)
        out[n++] = bottom;
    }
  }

  if (ix > sx) {
    PCRect left = {sx, iy, (uint32_t)(ix - sx), (uint32_t)(ib - iy)};
    if (left.width > 0 && left.height > 0) {
      if (n < out_cap)
        out[n++] = left;
    }
  }

  if (ir < sr) {
    PCRect right = {ir, iy, (uint32_t)(sr - ir), (uint32_t)(ib - iy)};
    if (right.width > 0 && right.height > 0) {
      if (n < out_cap)
        out[n++] = right;
    }
  }

  return n;
}

uint16_t PCRect_subtract_by_region(const PCRect *src, const PCRegion *cut,
                                   PCRect out[], uint16_t out_cap) {
  if (!src || out_cap == 0)
    return 0;

  if (rect_is_empty(src))
    return 0;

  if (!cut || (!cut->all && cut->count == 0)) {
    out[0] = *src;
    return 1;
  }

  if (cut->all) {
    return 0;
  }

  PCRect *cur = out;
  uint16_t cur_count = 1;
  cur[0] = *src;

  PCRect next_buf[out_cap];

  for (uint16_t j = 0; j < cut->count; ++j) {
    uint16_t next_count = 0;

    for (uint16_t i = 0; i < cur_count; ++i) {
      PCRect tmp[4];
      uint16_t tn = PCRect_subtract_by_rect(&cur[i], &cut->r[j], tmp, 4);

      for (uint16_t t = 0; t < tn; ++t) {
        if (next_count < out_cap) {
          next_buf[next_count++] = tmp[t];
        } else {
          break;
        }
      }

      if (next_count == out_cap)
        break;
    }

    for (uint16_t k = 0; k < next_count; ++k) {
      cur[k] = next_buf[k];
    }
    cur_count = next_count;

    if (cur_count == 0)
      break;
  }

  return cur_count;
}
