#include "mailbox.h"
#include <lib/dyn_array.h>
#include <lib/memory.h>
#include <lib/notification.h>
#include <physical_alloc.h>

#define MAILBOX_SIZE 8

RESULT_TYPE(mailbox_t *) make_mailbox() {
  mailbox_t *mb = (mailbox_t *)alloc_page();
  if (!mb) {
    return RESULT_FAILURE(RESULT_NOMEM);
  }
  memset(mb, 0, sizeof(mailbox_t));

  result_t rout = make_dyn_array(sizeof(notification_t), MAILBOX_SIZE);
  if (!result_is_ok(rout)) {
    free_page(mb);
    return RESULT_FAILURE(RESULT_NOMEM);
  }

  mb->incoming = (notification_queue_t *)result_unwrap(rout);

  initlock(&mb->lock, "mailbox");

  return RESULT_SUCCESS(mb);
}
