#include "panic.h"
#include <device/term.h>
#include <lib/ansi.h>

void panic(const char *msg) {
    term_puts(ANSI_EFFECT_BOLD);
    term_puts(ANSI_RGB_COLOR("231", "130", "132"));
    term_puts("PANIC: ");
    term_puts(ANSI_EFFECT_RESET);
    term_puts(msg);
    term_puts("\n");

    for (;;) {
        asm ("wfi");
    }
}
