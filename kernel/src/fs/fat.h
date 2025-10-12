#pragma once

#include <device/disk.h>
#include <lib/types.h>

/* Print a simple listing of the FAT12/16 root directory to console. */
g_bool fat_list_root(disk_t *disk);


