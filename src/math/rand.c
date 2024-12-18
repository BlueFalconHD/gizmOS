#include "rand.h"
#include "../device/rtc.h"

#include <stdint.h>


static uint64_t seed = 0;

void srand(uint64_t new_seed) {
    seed = new_seed;
}

uint32_t rand(void) {
    seed = seed * 6364136223846793005ULL + 1;
    return (uint32_t)(seed >> 32);
}

void init_rand(void) {
    uint64_t rtc_value = read_cntpct();
    srand(rtc_value);
}
