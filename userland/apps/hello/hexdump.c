// Minimal hexdump implementation for userland without stdio
#include "hexdump.h"
#include "../../sys/syscall.h"

static long g_out_fd = 0;

void hexdump_set_out(long handle) {
  if (handle >= 0) g_out_fd = handle;
}

static inline void print_char(char c) {
  char s[2] = {c, '\0'};
  (void)g_out_fd;
  sys_print_str(s);
}

static void print_str(const char *s) {
  long n = 0;
  while (s[n]) n++;
  (void)g_out_fd;
  if (n > 0) sys_print_str(s);
}

static inline char hex_digit(unsigned x) {
  return (x < 10) ? (char)('0' + x) : (char)('a' + (x - 10));
}

static void print_hex_u32_padded8(unsigned long long v) {
  unsigned long long val = v & 0xffffffffull;
  for (int shift = 28; shift >= 0; shift -= 4) {
    char d = hex_digit((unsigned)((val >> shift) & 0xf));
    print_char(d);
  }
}

static void print_hex_u8(unsigned char b) {
  char out[2];
  out[0] = hex_digit((unsigned)((b >> 4) & 0xf));
  out[1] = hex_digit((unsigned)(b & 0xf));
  char s[3];
  s[0] = out[0];
  s[1] = out[1];
  s[2] = '\0';
  sys_print_str(s);
}

void hexdump(const void *data, unsigned long long size, hexdump_opts_t opts) {
  const unsigned char *byte_data = (const unsigned char *)data;
  for (unsigned long long i = 0; i < size; i += (unsigned long long)opts.bytes_per_line) {
    if (opts.show_offset) {
      print_hex_u32_padded8(i);
      print_str("  ");
    }
    for (unsigned long long j = 0; j < (unsigned long long)opts.bytes_per_line; j++) {
      if ((opts.group_size > 0) && (j % (unsigned long long)opts.group_size == 0) && j != 0) {
        print_char(' ');
      }
      if (i + j < size) {
        print_hex_u8(byte_data[i + j]);
        print_char(' ');
      } else {
        print_str("   ");
      }
    }
    if (opts.show_ascii) {
      print_str(" |");
      for (unsigned long long j = 0; j < (unsigned long long)opts.bytes_per_line; j++) {
        if (i + j < size) {
          unsigned char byte = byte_data[i + j];
          char c = (byte >= 32 && byte <= 126) ? (char)byte : '.';
          print_char(c);
        } else {
          print_char(' ');
        }
      }
      print_char('|');
    }
    print_char('\n');
  }
}
