#include "../../sys/syscall.h"
#include <stddef.h>
#include <stdint.h>

// function to convert integer to string
static void int_to_str(int num, char *str, size_t cap) {
  if (cap == 0) return;
  size_t i = 0;
  int is_negative = 0;
  if (num < 0) {
    is_negative = 1;
    num = -num;
  }
  do {
    if (i + 1 >= cap) break;
    str[i++] = (char)('0' + (num % 10));
    num /= 10;
  } while (num > 0);
  if (is_negative) {
    if (i + 1 < cap) {
      str[i++] = '-';
    }
  }
  // Reverse the string
  for (size_t j = 0; j < i / 2; j++) {
    char tmp = str[j];
    str[j] = str[i - j - 1];
    str[i - j - 1] = tmp;
  }
  str[i] = '\0';
}

// take arguments
int main(int argc, char **argv) {
  // print argc
  sys_print_str("Argument count: ");
  char argc_str[20];
  int_to_str(argc, argc_str, sizeof(argc_str));
  sys_print_str(argc_str);
  sys_print_str("\n");

  for (int i = 1; i <= 10; i++) {
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
