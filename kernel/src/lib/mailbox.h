#pragma once

#include <lib/types.h>
#include <lib/notification.h>

struct mailbox {
    notification_queue_t outgoing;
    g_bool (*recieve)(notification_t *notification);
};

typedef struct mailbox mailbox_t;

RESULT_TYPE(mailbox_t*) make_mailbox();
