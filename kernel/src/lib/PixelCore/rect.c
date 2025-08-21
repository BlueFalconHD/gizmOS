#include "rect.h"

#include <lib/macros.h>
#include <lib/types.h>
#include <stdbool.h>
#include <stddef.h>

/**
 * Initializes a rectangle with the given parameters.
 *
 * @param rect Pointer to the rectangle to initialize.
 * @param x X coordinate of the rectangle.
 * @param y Y coordinate of the rectangle.
 * @param width Width of the rectangle.
 * @param height Height of the rectangle.
 */
void PCRect_init(PCRect *rect, int32_t x, int32_t y, uint32_t width,
                 uint32_t height) {
  rect->x = x;
  rect->y = y;
  rect->width = width;
  rect->height = height;
}

/**
 * Checks if two rectangles are equal.
 *
 * @param rect1 Pointer to the first rectangle.
 * @param rect2 Pointer to the second rectangle.
 */
bool PCRect_equal(const PCRect *rect1, const PCRect *rect2) {
  if (rect1->x == rect2->x && rect1->y == rect2->y &&
      rect1->width == rect2->width && rect1->height == rect2->height) {
    return true;
  }
  return false;
}

/**
 * Checks if a point is inside a rectangle.
 *
 * @param rect Pointer to the rectangle.
 * @param x X coordinate of the point.
 * @param y Y coordinate of the point.
 * @return 1 if the point is inside the rectangle, 0 otherwise.
 */
bool PCRect_contains(const PCRect *r, int32_t x, int32_t y) {
  int64_t rx0 = (int64_t)r->x, ry0 = (int64_t)r->y;
  int64_t rx1 = rx0 + (int64_t)r->width, ry1 = ry0 + (int64_t)r->height;
  return (x >= rx0) && (x < rx1) && (y >= ry0) && (y < ry1);
}

/**
 * Checks if two rectangles intersect.
 *
 * @param rect1 Pointer to the first rectangle.
 * @param rect2 Pointer to the second rectangle.
 * @return 1 if the rectangles intersect, 0 otherwise.
 */
bool PCRect_intersects(const PCRect *a, const PCRect *b) {
  int64_t ax0 = (int64_t)a->x, ay0 = (int64_t)a->y;
  int64_t ax1 = ax0 + (int64_t)a->width, ay1 = ay0 + (int64_t)a->height;
  int64_t bx0 = (int64_t)b->x, by0 = (int64_t)b->y;
  int64_t bx1 = bx0 + (int64_t)b->width, by1 = by0 + (int64_t)b->height;
  return (ax0 < bx1) && (bx0 < ax1) && (ay0 < by1) && (by0 < ay1);
}

/**
 * Calculates the intersection of two rectangles.
 *
 * @param rect1 Pointer to the first rectangle.
 * @param rect2 Pointer to the second rectangle.
 * @param result Pointer to the rectangle to store the result.
 * @return 1 if the rectangles intersect, 0 otherwise.
 */
bool PCRect_intersection(const PCRect *a, const PCRect *b, PCRect *out) {
  if (!a || !b || !out)
    return false;

  int64_t ax0 = (int64_t)a->x;
  int64_t ay0 = (int64_t)a->y;
  int64_t ax1 = ax0 + (int64_t)a->width;
  int64_t ay1 = ay0 + (int64_t)a->height;

  int64_t bx0 = (int64_t)b->x;
  int64_t by0 = (int64_t)b->y;
  int64_t bx1 = bx0 + (int64_t)b->width;
  int64_t by1 = by0 + (int64_t)b->height;

  int64_t ix0 = ax0 > bx0 ? ax0 : bx0;
  int64_t iy0 = ay0 > by0 ? ay0 : by0;
  int64_t ix1 = ax1 < bx1 ? ax1 : bx1;
  int64_t iy1 = ay1 < by1 ? ay1 : by1;

  if (ix1 > ix0 && iy1 > iy0) {
    out->x = (int32_t)ix0;
    out->y = (int32_t)iy0;
    out->width = (uint32_t)(ix1 - ix0);
    out->height = (uint32_t)(iy1 - iy0);
    return true;
  }
  out->x = 0;
  out->y = 0;
  out->width = 0;
  out->height = 0;
  return false;
}

/**
 * Resizes a rectangle to new dimensions.
 *
 * @param rect Pointer to the rectangle to resize.
 * @param new_width New width of the rectangle.
 * @param new_height New height of the rectangle.
 *
 * @return true if the rectangle was resized, false if the new dimensions are
 * invalid.
 */
bool PCRect_resize(PCRect *rect, uint32_t new_width, uint32_t new_height) {
  if (new_width == 0 || new_height == 0) {
    return false;
  }
  rect->width = new_width;
  rect->height = new_height;
  return true;
}

/**
 * Repositions a rectangle to new coordinates.
 *
 * @param rect Pointer to the rectangle to reposition.
 * @param new_x New X coordinate of the rectangle.
 * @param new_y New Y coordinate of the rectangle.
 */
void PCRect_reposition(PCRect *rect, int32_t new_x, int32_t new_y) {
  rect->x = new_x;
  rect->y = new_y;
}

/**
 * Unions two rectangles, creating a new rectangle that encompasses both.
 *
 * @param a Pointer to the first rectangle.
 * @param b Pointer to the second rectangle.
 * @param out Pointer to the rectangle to store the result.
 *
 * The resulting rectangle will have its top-left corner at the minimum x and y
 * coordinates of both rectangles, and its width and height will be the maximum
 * extents of both rectangles.
 */
void PCRect_union_bounds(const PCRect *a, const PCRect *b, PCRect *out) {
  int64_t ax0 = (int64_t)a->x, ay0 = (int64_t)a->y;
  int64_t ax1 = ax0 + (int64_t)a->width, ay1 = ay0 + (int64_t)a->height;
  int64_t bx0 = (int64_t)b->x, by0 = (int64_t)b->y;
  int64_t bx1 = bx0 + (int64_t)b->width, by1 = by0 + (int64_t)b->height;

  int64_t ux0 = ax0 < bx0 ? ax0 : bx0;
  int64_t uy0 = ay0 < by0 ? ay0 : by0;
  int64_t ux1 = ax1 > bx1 ? ax1 : bx1;
  int64_t uy1 = ay1 > by1 ? ay1 : by1;

  out->x = (int32_t)ux0;
  out->y = (int32_t)uy0;
  out->width = (uint32_t)(ux1 - ux0);
  out->height = (uint32_t)(uy1 - uy0);
}

#define CLOSENESS_THRESHOLD 5

/**
 * Merges two rectangles if they are close enough.
 *
 * @param a Pointer to the first rectangle.
 * @param b Pointer to the second rectangle.
 * @param out Pointer to the rectangle to store the result.
 * @return 1 if the rectangles were merged, 0 otherwise.
 *
 * The rectangles are considered close enough if they are adjacent or overlap.
 */
bool PCRect_merge_if_close(const PCRect *a, const PCRect *b, PCRect *out) {
  if (PCRect_intersects(a, b)) {
    PCRect_union_bounds(a, b, out);
    return true;
  }

  // Check if they are close enough to merge
  if ((a->x + a->width + CLOSENESS_THRESHOLD >= b->x) &&
      (a->y + a->height + CLOSENESS_THRESHOLD >= b->y) &&
      (b->x + b->width + CLOSENESS_THRESHOLD >= a->x) &&
      (b->y + b->height + CLOSENESS_THRESHOLD >= a->y)) {
    PCRect_union_bounds(a, b, out);
    return true;
  }

  return false;
}
