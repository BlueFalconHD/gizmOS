#include "../../sys/syscall.h"

int main(void) {
  for (int i = 1; i <= 100; i++) {
    if (i % 3 == 0 && i % 5 == 0) {
      sys_print_str("FizzBuzz\n");
    } else if (i % 3 == 0) {
      sys_print_str("Fizz\n");
    } else if (i % 5 == 0) {
      sys_print_str("Buzz\n");
    }
  }

  return 0;
}
