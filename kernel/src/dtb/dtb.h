#ifndef DTB_H
#define DTB_H 1

#include <stdint.h>
#include <stdbool.h>
#include "smoldtb/smoldtb.h"
#include "../memory.h"

void on_error(const char* why);

static dtb_ops gizmOS_dtb_ops = {
    .malloc = NULL,
    .free = NULL,
    .on_error = on_error
};

bool init_dtb(uintptr_t start);

#endif /* DTB_H */
