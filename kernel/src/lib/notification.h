#pragma once

// #include "proc.h"
#include <lib/dyn_array.h>
#include <stdint.h>

typedef enum notification_type {
  NOTIFICATION_TYPE_NONE = 0,
  NOTIFICATION_TYPE_UART = 1,
  NOTIFICATION_TYPE_KEYBOARD = 2,
  NOTIFICATION_TYPE_MOUSE = 3,
  NOTIFICATION_TYPE_GIZMO = 4
} notification_type_t;

typedef struct notification {
    uint64_t token; /* A unique, randomly generated identifier for the notification. When performing actions on a notification (e.g. determining who sent it), this token is used rather than the notification itself. */
    notification_type_t type;
    uint32_t data_size;
    void *data;
} notification_t;

// forward declaration of proc_t
typedef struct proc proc_t;

typedef struct notification_ledger_entry {
    uint64_t token;               /* A unique, randomly generated identifier for the notification. When performing actions on a notification (e.g. determining who sent it), this token is used rather than the notification itself. */

    proc_t *sender;               /* If this is a null pointer, sender is kernel */
    proc_t *reciever;             /* If this is a null pointer, reciever is kernel */
    notification_t *notification; /* The notification itself */
    uint64_t timestamp;           /* The time the notification was sent */
} notification_ledger_entry_t;

DYN_ARRAY_DECLARE(notification_t);
typedef DYN_ARRAY_TYPE(notification_t) notification_queue_t;

DYN_ARRAY_DECLARE(notification_ledger_entry_t);
typedef DYN_ARRAY_TYPE(notification_ledger_entry_t) notification_ledger_t;
