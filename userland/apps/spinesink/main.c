#include "../../sys/syscall.h"
#include <stdint.h>
#include <stdbool.h>

// Must match kernel wire header
typedef struct __attribute__((packed)) {
  uint64_t sender_token;
  uint32_t message_size;
  uint32_t reserved;
} spine_wire_msg_t;

static void __attribute__((noreturn)) spin(void) {
  for (;;) {
  }
}

static void print_chunked(const char *s, unsigned long n) {
  // Print n bytes from s as ASCII via sys_print_str in small chunks
  char buf[256 + 1];
  while (n > 0) {
    unsigned long k = n < 256 ? n : 256;
    for (unsigned long i = 0; i < k; i++) buf[i] = s[i];
    buf[k] = '\0';
    sys_print_str(buf);
    s += k;
    n -= k;
  }
}

static void
spine_handler(uint64_t type, uint64_t payload_uva, uint64_t len, uint64_t arg) {
  (void)type; (void)arg;
  if (len < sizeof(spine_wire_msg_t)) {
    sys_print_str("[spinesink] short message\n");
    return;
  }
  spine_wire_msg_t *hdr = (spine_wire_msg_t *)payload_uva;
  const char *msg = (const char *)(payload_uva + sizeof(spine_wire_msg_t));
  unsigned long avail = len - sizeof(spine_wire_msg_t);
  unsigned long mlen = hdr->message_size;
  if (mlen > avail) mlen = avail;

  // Optional: print sender info
  sys_spine_seal_t seal;
  long seal_ok = sys_spine_get_seal(hdr->sender_token, &seal, (long)sizeof(seal));
  if (seal_ok == 0) {
    sys_print_str("[spinesink from ");
    // print pid
    // reuse tiny decimal printer
    {
      char tmp[32];
      long v = (long)seal.pid;
      int i = 0;
      if (v == 0) tmp[i++] = '0';
      char rev[32]; int ri = 0;
      while (v > 0 && ri < (int)sizeof(rev)) { rev[ri++] = (char)('0' + (v % 10)); v /= 10; }
      while (ri > 0 && i < (int)sizeof(tmp)) tmp[i++] = rev[--ri];
      tmp[i] = '\0';
      sys_print_str(tmp);
    }
    sys_print_str(" ");
    sys_print_str(seal.service);
    sys_print_str("] ");
  } else {
    sys_print_str("[spinesink] ");
  }

  print_chunked(msg, mlen);
  sys_print_str("\n");
}

int main(void) {
  // Advertise service name so senders can find us easily
  long adv = sys_spine_service_advertise("spinesink", 0);
  (void)adv;
  sys_notif_register(2 /* NOTIF_TYPE_SPINE_MESSAGE */, (uint64_t)&spine_handler, 0, 0);
  spin();
}


