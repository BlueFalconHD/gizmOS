#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/** Component structure */
struct request_t {
    const char *name; /** Component name */
    bool (*init)(); /** Component initialization function */
};

/** All defined components */
extern struct request_t REQUESTS[];
