#pragma once

#include <device/disk.h>
#include <lib/types.h>

/* Print a simple listing of the FAT12/16 root directory to console. */
g_bool fat_list_root(disk_t *disk);

// Minimal helpers for reading a full file into a kernel buffer from the FAT root
// path naming uses ::/NAME.EXT (8.3). Returns false if not found or buffer too small.
g_bool fat_read_root_file(disk_t *disk, const char *name83, void *dst, uint32_t max_bytes, uint32_t *out_size);


