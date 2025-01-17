#ifndef DTB_H
#define DTB_H 1

#include <stdint.h>
#include <stdbool.h>
#include <extern/smoldtb/smoldtb.h>
#include <memory.h>

void on_error(const char* why);

static dtb_ops gizmOS_dtb_ops = {
    .malloc = NULL,
    .free = NULL,
    .on_error = on_error
};

void dtb_init();

#endif /* DTB_H */
