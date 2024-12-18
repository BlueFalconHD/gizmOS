#include "rtc.h"

uint64_t read_cntpct(void) {
    uint64_t cntpct;
    asm volatile("mrs %0, cntpct_el0" : "=r" (cntpct));
    return cntpct;
}
