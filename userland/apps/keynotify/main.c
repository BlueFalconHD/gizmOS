#include <stdint.h>
#include "../../sys/syscall.h"

static void __attribute__((noreturn)) spin(void) { for(;;){} }

static void key_handler(uint64_t type, uint64_t payload_uva, uint64_t len, uint64_t arg) {
  (void)type;
  (void)arg;
  if (len >= 4) {
    // payload[3] == 1 filter (press)
    uint8_t *p = (uint8_t *)payload_uva;
    if (p[3] == 1) {
      sys_print_int((long)p[0]);
    }
  }
  // Return by ecall with a7 pre-set by kernel to SYSCALL_NOTIF_DONE
}

int main(void) {
  sys_notif_register(1 /* keypress */, (uint64_t)&key_handler, 0, 0);
  spin();
}


