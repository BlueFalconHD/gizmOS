#include "../../sys/syscall.h"
#include <stdint.h>
#include <stdbool.h>

// Must match kernel wire header
typedef struct __attribute__((packed)) {
  uint64_t sender_token;
  uint32_t message_size;
  uint32_t reserved;
} spine_wire_msg_t;

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

static void spine_print_payload(uint64_t sender_token, const uint8_t *payload, unsigned long payload_len) {
  const char *msg = (const char *)payload;
  unsigned long mlen = payload_len;

  // Optional: print sender info
  sys_spine_seal_t seal;
  long seal_ok = sys_spine_get_seal(sender_token, &seal, (long)sizeof(seal));
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

  enum { SPINE_RECV_BUF_SIZE = 64 * 1024 };
  static uint8_t rxbuf[SPINE_RECV_BUF_SIZE];

  for (;;) {
    long got = 0;
    uint64_t sender_token = 0;
    long rc = sys_spine_msg_recv(rxbuf, (long)sizeof(rxbuf), &got, &sender_token, 0);
    if (rc == 0 && got > 0) {
      spine_print_payload(sender_token, rxbuf, (unsigned long)got);
    }
  }
}
