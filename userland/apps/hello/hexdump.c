// Minimal hexdump implementation for userland without stdio
#include "hexdump.h"

// Inline the minimal syscall we need to avoid pulling in stdint.h
static inline long sys_write(long fd, const void *buf, long n) {
  register long a0 asm("a0") = fd;
  register long a1 asm("a1") = (long)buf;
  register long a2 asm("a2") = n;
  register long a7 asm("a7") = 0x205; /* SYSCALL_WRITE */
  asm volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a2), "r"(a7) : "memory");
  return a0;
}

static inline void print_char(char c) {
  sys_write(1, &c, 1);
}

static void print_str(const char *s) {
  long n = 0;
  while (s[n]) n++;
  if (n > 0) sys_write(1, s, n);
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
  sys_write(1, out, 2);
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
