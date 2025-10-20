#include "../../sys/syscall.h"

int main(void) {
  for (int i = 0; i < 100000; i++) {
    if (i % 5000 == 0)
      sys_print_int(i);
  }
  return 0;
}
