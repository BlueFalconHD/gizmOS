#include "mailbox.h"
#include <lib/dyn_array.h>
#include <lib/memory.h>
#include <lib/notification.h>
#include <physical_alloc.h>

RESULT_TYPE(mailbox_t *) make_mailbox(g_bool (*recieve)(notification_t *)) {
  struct mailbox *mb = (struct mailbox *)alloc_page();
  if (!mb) {
    return RESULT_FAILURE(RESULT_NOMEM);
  }
  memset(mb, 0, sizeof(struct mailbox));
  dyn_array_init(&mb->outgoing, sizeof(notification_t), 8);

  mb->recieve = recieve;

  return RESULT_SUCCESS(mb);
}

