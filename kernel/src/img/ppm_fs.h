#pragma once

#include <device/disk.h>
#include <device/framebuffer.h>
#include <lib/PixelCore/surface.h>
#include <lib/types.h>

/* Load and draw PPM (P6) images from a FAT root filesystem into framebuffer or
 * surfaces. */

/*
 * Read a PPM from the FAT root (8.3 name) and blit to framebuffer at (dst_x,
 * dst_y).
 */
g_bool ppm_blit_fat_root_to_framebuffer(disk_t *disk, const char *name83,
                                        framebuffer_t *fb, uint32_t dst_x,
                                        uint32_t dst_y);

/*
 * Read a PPM from the FAT root (8.3 name) and blit into an existing surface at
 * (dst_x, dst_y).
 */
g_bool ppm_blit_fat_root_to_surface(disk_t *disk, const char *name83,
                                    PCSurface *surface, int32_t dst_x,
                                    int32_t dst_y);

/*
 * Read a PPM from the FAT root (8.3 name) and create a new surface sized to the
 * image. Returns NULL on failure.
 */
PCSurface *ppm_load_surface_from_fat_root(disk_t *disk, const char *name83,
                                          uint32_t z_index);

/* Query dimensions of a PPM in the FAT root (8.3 name). */
g_bool ppm_fat_root_get_dimensions(disk_t *disk, const char *name83,
                                   uint32_t *out_w, uint32_t *out_h);
