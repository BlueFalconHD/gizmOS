#include "../../sys/syscall.h"
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

int main(void) {
  sys_print_str("Listing /\n");
  sys_dirent_t ents[16];
  long n = sys_getdents("/", ents, sizeof(ents));
  if (n > 0) {
    long cnt = n / (long)sizeof(sys_dirent_t);
    for (long i = 0; i < cnt; i++) {
      sys_print_str(" - ");
      sys_print_str(ents[i].name);
      sys_print_str("\n");
    }
  }

  long fd = sys_open("/HELLO.VES", 0);

  if (fd >= 0) {
    char buf[64];
    long r = sys_read(fd, buf, sizeof(buf));
    if (r > 0) {
      hexdump(buf, (unsigned long long)r, hexdump_opts);
    }
    sys_close(fd);
  }

  // Try listing and opening a resource fork if present
  sys_dirent_t forks[8];
  long fn = sys_fork_list("/HELLO.VES", forks, sizeof(forks));
  if (fn > 0) {
    long fcnt = fn / (long)sizeof(sys_dirent_t);
    for (long i = 0; i < fcnt; i++) {
      long ffd = sys_fork_open("/HELLO.VES", forks[i].name, 0);
      sys_write(1, "Opened fork: ", 13);
      sys_write(1, forks[i].name, sstrlen(forks[i].name));
      sys_write(1, "\n", 1);
      if (ffd >= 0) {
        char fb[32];
        long fr = sys_read(ffd, fb, sizeof(fb));
        if (fr > 0) {
          hexdump(fb, (unsigned long long)fr, hexdump_opts);
        }
        sys_close(ffd);
      }
    }
  }
  return 0;
}
