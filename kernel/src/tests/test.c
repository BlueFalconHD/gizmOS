#include "test.h"
#include <lib/print.h>

void test_complete(const char *name, bool result) {
  if (!result) {
    print("test ", PRINT_FLAG_BOTH);
    print(name, PRINT_FLAG_BOTH);
    print(" ", PRINT_FLAG_BOTH);
    // term_puts(ANSI_RED);
    print("failed", PRINT_FLAG_BOTH);
    // term_puts(ANSI_RESET);
    print("\n", PRINT_FLAG_BOTH);
  }
}
