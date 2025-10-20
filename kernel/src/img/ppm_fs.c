#include "ppm_fs.h"

#include <fs/fat.h>
#include <lib/kalloc.h>
#include <lib/memory.h>

#include "ppm.h"

static void *ppm_read_fat_root_file(disk_t *disk, const char *name83,
                                    uint32_t *out_size) {
  if (!disk || !disk->is_initialized || !name83)
    return NULL;

  uint32_t size = 0;
  /* First pass: query size */
  if (!fat_read_root_file(disk, name83, NULL, 0, &size)) {
    if (size == 0)
      return NULL;
  }

  void *buf = kalloc(size);
  if (!buf)
    return NULL;

  uint32_t got = 0;
  if (!fat_read_root_file(disk, name83, buf, size, &got)) {
    kfree(buf);
    return NULL;
  }
  if (out_size)
    *out_size = got;
  return buf;
}

g_bool ppm_blit_fat_root_to_framebuffer(disk_t *disk, const char *name83,
                                        framebuffer_t *fb, uint32_t dst_x,
                                        uint32_t dst_y) {
  uint32_t size = 0;
  void *data = ppm_read_fat_root_file(disk, name83, &size);
  if (!data)
    return false;
  g_bool ok = ppm_blit_to_framebuffer(fb, dst_x, dst_y, data, size);
  kfree(data);
  return ok;
}

g_bool ppm_blit_fat_root_to_surface(disk_t *disk, const char *name83,
                                    PCSurface *surface, int32_t dst_x,
                                    int32_t dst_y) {
  uint32_t size = 0;
  void *data = ppm_read_fat_root_file(disk, name83, &size);
  if (!data)
    return false;
  g_bool ok = ppm_blit_to_surface(surface, dst_x, dst_y, data, size);
  kfree(data);
  return ok;
}

PCSurface *ppm_load_surface_from_fat_root(disk_t *disk, const char *name83,
                                          uint32_t z_index) {
  uint32_t size = 0;
  void *data = ppm_read_fat_root_file(disk, name83, &size);
  if (!data)
    return NULL;
  PCSurface *s = ppm_create_surface(data, size, z_index);
  kfree(data);
  return s;
}

g_bool ppm_fat_root_get_dimensions(disk_t *disk, const char *name83,
                                   uint32_t *out_w, uint32_t *out_h) {
  uint32_t size = 0;
  void *data = ppm_read_fat_root_file(disk, name83, &size);
  if (!data)
    return false;
  ppm_info_t info;
  g_bool ok = ppm_parse(data, size, &info);
  if (ok) {
    if (out_w)
      *out_w = info.width;
    if (out_h)
      *out_h = info.height;
  }
  kfree(data);
  return ok;
}
