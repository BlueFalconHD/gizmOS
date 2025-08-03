#pragma once

#include <lib/spinlock.h>
#include <lib/types.h>
#include <lib/notification.h>

typedef struct mailbox {
    notification_queue_t* incoming;
    struct spinlock lock;
} mailbox_t;

RESULT_TYPE(mailbox_t*) make_mailbox();
