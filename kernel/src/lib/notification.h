#pragma once

#include <lib/dyn_array.h>
#include <stdint.h>

enum notification_type {
  NOTIFICATION_TYPE_NONE = 0,
  NOTIFICATION_TYPE_UART = 1,
  NOTIFICATION_TYPE_KEYBOARD = 2,
  NOTIFICATION_TYPE_MOUSE = 3,
  NOTIFICATION_TYPE_GIZMO = 4
};

struct notification {
    enum notification_type type;
    uint32_t data_size;
    void *data;
};

typedef struct notification notification_t;

DYN_ARRAY_DECLARE(notification_t);

typedef DYN_ARRAY_TYPE(notification_t) notification_queue_t;
