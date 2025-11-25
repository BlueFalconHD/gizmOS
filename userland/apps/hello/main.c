#include "../../sys/syscall.h"
#include "../../libc-lite/obj.h"
#include "hexdump.h"

static const hexdump_opts_t hexdump_opts = {
    .bytes_per_line = 16, .show_ascii = 1, .group_size = 8};

int sstrlen(const char *s) {
  int len = 0;
  while (s[len] != '\0') {
    len++;
  }
  return len;
}

static inline void puts_out(long out, const char *s) {
  (void)out;
  if (s) {
    sys_print_str(s);
  }
}

int main(void) {
  long out = -1;
  puts_out(out, "Listing /\n");
  long root_h = obj_open("/", 0);
  sys_print_str("hello: obj_open(/) -> ");
  sys_print_int(root_h);
  sys_print_str("\n");
  if (root_h >= 0) {
    sys_objdirent_t ents[32];
    long n = obj_getdents(root_h, ents, sizeof(ents));
    sys_print_str("hello: obj_getdents(/) bytes -> ");
    sys_print_int(n);
    sys_print_str("\n");
    if (n > 0) {
      long cnt = n / (long)sizeof(sys_objdirent_t);
      for (long i = 0; i < cnt; i++) {
        puts_out(out, " - ");
        puts_out(out, ents[i].name);
        puts_out(out, "\n");
      }
    }
    obj_close(root_h);
  } else {
    puts_out(out, "Failed to open '/' as handle\n");
  }

  long file_h = obj_open("/HELLO.VES", 0);
  sys_print_str("hello: obj_open(HELLO.VES) -> ");
  sys_print_int(file_h);
  sys_print_str("\n");

  if (file_h >= 0) {
    hexdump_set_out(out >= 0 ? out : 0);
    char buf[64];
    long r = obj_pread(file_h, buf, 0, sizeof(buf));
    sys_print_str("hello: obj_pread(HELLO.VES) -> ");
    sys_print_int(r);
    sys_print_str("\n");
    if (r > 0) {
      hexdump(buf, (unsigned long long)r, hexdump_opts);
    }
    obj_close(file_h);
  } else {
    puts_out(out, "HELLO.VES not found in ObjectFS\n");
  }
  return 0;
}
