#ifndef DTB_H
#define DTB_H 1

#include <extern/smoldtb/smoldtb.h>
#include <memory.h>
#include <stdbool.h>
#include <stdint.h>

void on_error(const char *why);

static dtb_ops gizmOS_dtb_ops = {
    .malloc = NULL, .free = NULL, .on_error = on_error};

void dtb_init();

void dtb_dostuff();

extern dtb_node *root_node;

#endif /* DTB_H */
