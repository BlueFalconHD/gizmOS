#include "../../sys/syscall.h"
#include "virtio_keycode.h"
#include <stdint.h>

static void __attribute__((noreturn)) spin(void) {
  for (;;) {
  }
}

static void __attribute__((noreturn))
key_handler(uint64_t type, uint64_t payload_uva, uint64_t len, uint64_t arg) {
  (void)type;
  (void)arg;
  if (len >= 4) {
    uint8_t *p = (uint8_t *)payload_uva;
    if (p[3] == 1) {
      sys_print_str(virtio_keycode_to_string(p[0]));
    }
  }

  sys_notif_done();
  __builtin_unreachable();
}

int main(void) {
  sys_notif_register(1 /* keypress */, (uint64_t)&key_handler, 0, 0);
  spin();
}
