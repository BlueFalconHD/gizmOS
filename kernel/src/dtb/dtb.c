#include "dtb.h"
#include <stdint.h>
#include "smoldtb/smoldtb.h"
#include "../device/term.h"


void hcf() {
    for (;;) {
        asm ("wfi");
    }
}

void on_error(const char* why) {
    print_error("smoldtb error:");
    print_error(why);
    hcf();
}

bool init_dtb(uintptr_t start) {
    return dtb_init(start, gizmOS_dtb_ops);
}
