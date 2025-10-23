#pragma once

typedef struct {
  char show_ascii;
  char show_offset;
  int bytes_per_line;
  int group_size;
} hexdump_opts_t;

void hexdump(const void *data, unsigned long long size, hexdump_opts_t opts);
