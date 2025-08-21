#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct PCRect {
  int32_t x;       // X coordinate of the rectangle
  int32_t y;       // Y coordinate of the rectangle
  uint32_t width;  // Width of the rectangle
  uint32_t height; // Height of the rectangle
} PCRect;

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
                 uint32_t height);

/**
 * Checks if two rectangles are equal.
 *
 * @param rect1 Pointer to the first rectangle.
 * @param rect2 Pointer to the second rectangle.
 */
bool PCRect_equal(const PCRect *rect1, const PCRect *rect2);

/**
 * Checks if a point is inside a rectangle.
 *
 * @param rect Pointer to the rectangle.
 * @param x X coordinate of the point.
 * @param y Y coordinate of the point.
 * @return 1 if the point is inside the rectangle, 0 otherwise.
 */
bool PCRect_contains(const PCRect *rect, int32_t x, int32_t y);

/**
 * Checks if two rectangles intersect.
 *
 * @param rect1 Pointer to the first rectangle.
 * @param rect2 Pointer to the second rectangle.
 * @return 1 if the rectangles intersect, 0 otherwise.
 */
bool PCRect_intersects(const PCRect *rect1, const PCRect *rect2);

/**
 * Calculates the intersection of two rectangles.
 *
 * @param rect1 Pointer to the first rectangle.
 * @param rect2 Pointer to the second rectangle.
 * @param result Pointer to the rectangle to store the result.
 * @return 1 if the rectangles intersect, 0 otherwise.
 */
bool PCRect_intersection(const PCRect *rect1, const PCRect *rect2,
                         PCRect *result);

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
bool PCRect_resize(PCRect *rect, uint32_t new_width, uint32_t new_height);

/**
 * Repositions a rectangle to new coordinates.
 *
 * @param rect Pointer to the rectangle to reposition.
 * @param new_x New X coordinate of the rectangle.
 * @param new_y New Y coordinate of the rectangle.
 */
void PCRect_reposition(PCRect *rect, int32_t new_x, int32_t new_y);

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
bool PCRect_merge_if_close(const PCRect *a, const PCRect *b, PCRect *out);

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
void PCRect_union_bounds(const PCRect *a, const PCRect *b, PCRect *out);
