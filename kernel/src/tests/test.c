#include "test.h"
#include "../device/term.h"

void test_complete(const char *name, bool result) {
    if (!result) {
        term_puts("test ");
        term_puts(name);
        term_puts(" ");
        term_puts(ANSI_RED);
        term_puts("failed");
        term_puts(ANSI_RESET);
        term_puts("\n");
    }
}
