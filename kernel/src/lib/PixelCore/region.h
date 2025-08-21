#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "rect.h"

/**
 * PCRegion - A structure representing a region of rectangles
 *
 * This structure represents a set of rectangles; when 'all' is true it
 * represents full coverage of the domain.
 */
typedef struct {
  PCRect *r;
  uint16_t count;
  uint16_t capacity;
  bool all; // when true, represents the whole surface->rect
} PCRegion;

/**
 * Initializes a PCRegion with the given storage and capacity.
 *
 * @param rg Pointer to the PCRegion to initialize.
 * @param storage Pointer to an array of PCRect to use as storage.
 * @param capacity The maximum number of rectangles the region can hold.
 */
void PCRegion_init(PCRegion *rg, PCRect *storage, uint16_t capacity);

/**
 * Clears all rectangles from the region.
 *
 * @param rg Pointer to the PCRegion to clear.
 */
void PCRegion_clear(PCRegion *rg);

/**
 * Adds a rectangle to the region, merging with existing rectangles if
 * at maximum capacity.
 *
 * @param rg Pointer to the PCRegion to modify.
 * @param rect Pointer to the PCRect to add.
 */
void PCRegion_add_coalescing(PCRegion *rg, const PCRect *rect);
void PCRegion_add_no_merge(PCRegion *rg, const PCRect *rect);

/**
 * Translate all rectangles in the region by the specified offsets and store
 * them in a new region; if 'in->all' is set, 'out->all' is set and no concrete
 * rects are emitted.
 *
 * @param in Pointer to the input PCRegion containing rectangles to translate.
 * @param out Pointer to the output PCRegion to store translated rectangles.
 * @param dx The horizontal offset to translate by.
 * @param dy The vertical offset to translate by.
 */
void PCRegion_translate(const PCRegion *in, PCRegion *out, int32_t dx,
                        int32_t dy);

/**
 * Intersect a region with a clip rectangle and store the result in the output.
 * If 'in->all' is set, the result is just the clip rectangle.
 *
 * @param in Pointer to the input PCRegion.
 * @param clip Pointer to the clipping rectangle.
 * @param out Pointer to the output PCRegion to store the intersection.
 */
void PCRegion_intersect_clip(const PCRegion *in, const PCRect *clip,
                             PCRegion *out);

/**
 * Subtract a single rectangle from a source rectangle.
 * Writes up to out_cap disjoint rectangles that cover src \ cut.
 * Returns the number of rectangles written (0..out_cap).
 *
 * @param src Pointer to the source rectangle.
 * @param cut Pointer to the rectangle to subtract.
 * @param out Array to receive resulting rectangles.
 * @param out_cap Capacity of the out array.
 */
uint16_t PCRect_subtract_by_rect(const PCRect *src, const PCRect *cut,
                                 PCRect out[], uint16_t out_cap);

/**
 * Subtract a region (set of rectangles) from a source rectangle.
 * Writes up to out_cap disjoint rectangles that cover src \ cut.
 * Returns the number of rectangles written (0..out_cap).
 *
 * @param src Pointer to the source rectangle.
 * @param cut Pointer to the region to subtract.
 * @param out Array to receive resulting rectangles.
 * @param out_cap Capacity of the out array.
 */
uint16_t PCRect_subtract_by_region(const PCRect *src, const PCRegion *cut,
                                   PCRect out[], uint16_t out_cap);
