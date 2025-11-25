// Simple userspace test for ObjectFS handle IO
#include "../../sys/syscall.h"
#include "../../libc-lite/obj.h"

int main(void) {
  long h = obj_open("/HELLO.VES", 0);
  if (h < 0) {
    sys_print_str("test_objio: open failed\n");
    return -1;
  }
  char buf[32];
  long n = obj_pread(h, buf, 0, sizeof(buf));
  if (n < 0) {
    sys_print_str("test_objio: read failed\n");
  } else {
    sys_print_str("test_objio: read ok\n");
  }
  obj_close(h);
  return 0;
}

