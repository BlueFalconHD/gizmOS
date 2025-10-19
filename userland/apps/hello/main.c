#include "../../sys/syscall.h"

int main(void) {
  for (int i = 0; i < 100; i++) {
    if (i % 2 == 0)
      sys_print_int(i);
  }
  return 0;
}
