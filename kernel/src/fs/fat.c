#include "fat.h"
#include <lib/print.h>
#include <lib/memory.h>
#include <lib/kalloc.h>

/* Very small FAT12/16 boot sector fields */
typedef struct __attribute__((packed)) {
  uint8_t  jmp[3];
  char     oem[8];
  uint16_t bytes_per_sector;
  uint8_t  sectors_per_cluster;
  uint16_t reserved_sectors;
  uint8_t  num_fats;
  uint16_t root_entries;
  uint16_t total_sectors16;
  uint8_t  media;
  uint16_t sectors_per_fat16;
  uint16_t sectors_per_track;
  uint16_t num_heads;
  uint32_t hidden_sectors;
  uint32_t total_sectors32;
} fat_bpb_t;

typedef struct __attribute__((packed)) {
  char name[8];
  char ext[3];
  uint8_t attrs;
  uint8_t reserved[10];
  uint16_t time;
  uint16_t date;
  uint16_t start_cluster;
  uint32_t size;
} fat_dirent_t;

static void print_name(const fat_dirent_t *de)
{
  char n[13];
  int i = 0;
  for (int k = 0; k < 8 && de->name[k] != ' '; k++) n[i++] = de->name[k];
  if (de->ext[0] != ' ')
    n[i++]='.', n[i++]=de->ext[0], n[i++]=de->ext[1], n[i++]=de->ext[2];
  n[i] = '\0';
  print(n, PRINT_FLAG_BOTH);
}

g_bool fat_list_root(disk_t *disk)
{
  if (!disk || !disk->is_initialized)
    return false;

  const uint32_t bs = disk->sector_size;
  uint8_t *sec = (uint8_t *)kalloc(bs);
  if (!sec) return false;

  if (!disk_read(disk, 0, sec, 1)) {
    kfree(sec);
    return false;
  }

  fat_bpb_t *bpb = (fat_bpb_t *)sec;
  uint32_t root_sectors = ((bpb->root_entries * 32) + (bs - 1)) / bs;
  uint32_t fat_sectors = bpb->sectors_per_fat16;
  uint32_t first_data_sector = bpb->reserved_sectors + (bpb->num_fats * fat_sectors) + root_sectors;
  (void)first_data_sector;
  uint32_t first_root_sector = bpb->reserved_sectors + (bpb->num_fats * fat_sectors);

  print("FAT root listing:\n", PRINT_FLAG_BOTH);

  uint32_t entries = bpb->root_entries;
  uint32_t ents_per_sector = bs / sizeof(fat_dirent_t);
  uint32_t sectors_to_scan = root_sectors;

  uint8_t *dirbuf = (uint8_t *)kalloc(bs);
  if (!dirbuf) { kfree(sec); return false; }

  for (uint32_t s = 0; s < sectors_to_scan; s++) {
    uint64_t lba = first_root_sector + s;
    if (!disk_read(disk, lba, dirbuf, 1)) break;
    fat_dirent_t *de = (fat_dirent_t *)dirbuf;
    for (uint32_t i = 0; i < ents_per_sector && entries; i++, entries--) {
      if (de[i].name[0] == 0x00) { entries = 0; break; }
      if ((uint8_t)de[i].name[0] == 0xE5) continue; /* deleted */
      if (de[i].attrs & 0x08) continue; /* volume label */
      if (de[i].name[0] == '.') continue; /* . or .. */
      print(" - ", PRINT_FLAG_BOTH);
      print_name(&de[i]);
      print("\n", PRINT_FLAG_BOTH);
    }
  }

  kfree(dirbuf);
  kfree(sec);
  return true;
}


