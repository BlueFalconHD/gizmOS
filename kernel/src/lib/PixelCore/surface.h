#pragma once

#include "../spinlock.h"
#include "rect.h"
#include <stdint.h>

/**
 * PCSurfaceBackendKind - Enum representing the backend type for a surface
 *
 * This enum defines the type of backend used for rendering the surface.
 */
typedef enum {
  PC_SURFACE_BACKEND_CPU = 0,
  PC_SURFACE_BACKEND_GPU = 1,
} PCSurfaceBackendKind;

/**
 * PCSurfaceFlags - Flags representing properties of a surface
 *
 * These flags indicate various properties of the surface, such as visibility
 * and opacity. They can be combined using bitwise OR operations.
 */
typedef enum {
  PC_SURFACE_VISIBLE = 1u << 0, // participates in composition
  PC_SURFACE_OPAQUE =
      1u << 1, // treat as fully covering when alpha=255 (helps occlusion)
} PCSurfaceFlags;

struct PCBackBuffer;

/**
 * PCSurface - A 2D pixel buffer with dirty region tracking
 *
 * This structure represents a 2D surface of pixels, along with an array
 * of dirty regions that have been modified and need to be redrawn.
 *
 * IMPORTANT: To resize, don't directly use PCRect_resize; instead, use
 * PCSurface_resize
 */
typedef struct PCSurface {
  PCRect rect;          // Dimensions of the surface
  PCRect previous_rect; // Previous dimensions for tracking changes

  uint32_t z_index; // Z-order for layering surfaces
  uint32_t id;      // Creation order tracking ID

  uint32_t *pixels; // Pixel data in 0xAARRGGBB format
  uint32_t stride;  // Stride in pixels (width in pixels)

  PCSurfaceBackendKind backend;
  void *backend_data; // backend specific data

  uint32_t flags; // PCSurfaceFlags
  bool in_scene;  // managed by back buffer?
  struct PCBackBuffer *owner;

  PCRect dirty_regions[8]; // Array of dirty regions
  uint8_t dirty_count;

  struct spinlock lock;
} PCSurface;

/**
 * Create a new surface with the specified width, height, and z-index.
 *
 * @param rect Dimensions of the surface
 * @param z_index Z-order index for layering
 *
 * @return Pointer to the newly created surface, or NULL on failure
 *
 * The returned surface must be freed with PCSurface_free when no longer needed.
 */
PCSurface *PCSurface_create(PCRect *rect, uint32_t z_index);

/**
 * Free the memory associated with a surface.
 *
 * @param surface The surface to free
 */
void PCSurface_free(PCSurface *surface);

/**
 * Mark a region of the surface as dirty, indicating it needs to be redrawn.
 *
 * @param surface The surface to mark dirty
 * @param region The region to mark as dirty
 */
void PCSurface_mark_dirty(PCSurface *surface, PCRect region);

/**
 * Clear all dirty regions on the surface.
 *
 * @param surface The surface to clear dirty regions
 */
void PCSurface_clear_dirty(PCSurface *surface);

/**
 * Resize the surface to new dimensions.
 *
 * @param surface The surface to resize
 * @param new_rect The new dimensions for the surface
 * @return true on success, false on failure
 */
bool PCSurface_resize(PCSurface *surface, uint32_t new_width,
                      uint32_t new_height);

/**
 * Reposition the surface to a new location.
 *
 * @param surface The surface to reposition
 * @param new_x The new X coordinate
 * @param new_y The new Y coordinate
 */
void PCSurface_reposition(PCSurface *surface, int32_t new_x, int32_t new_y);

/**
 * Set the Z-order index of the surface.
 *
 * @param surface The surface to update
 * @param new_z_index The new Z-order index
 */
void PCSurface_reorder(PCSurface *surface, uint32_t new_z_index);
