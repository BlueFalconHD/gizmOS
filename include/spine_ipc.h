#pragma once

#include <stdint.h>

#define SPINE_MSGF_SEND (1u << 0)
#define SPINE_MSGF_RECV (1u << 1)

// Receive returns payload bytes only (no wire header).
// Sender token (for replies) is returned via `sender_token_out` when non-zero.
#define SPINE_MSGF_RECV_BODY_ONLY (1u << 2)

typedef struct __attribute__((packed)) {
  int64_t  dest_pid;  // destination pid, or -1
  uint32_t flags;     // SPINE_MSGF_*
  uint32_t _pad0;
  uint64_t send_buf;  // user VA, or 0
  uint64_t send_len;  // bytes

  uint64_t recv_buf;         // user VA, or 0
  uint64_t recv_cap;         // bytes
  uint64_t recv_len_out;     // user VA to u64, optional (0 = ignore)
  uint64_t sender_token_out; // user VA to u64, optional (0 = ignore)
  uint64_t timeout_ticks;    // 0 = wait forever
} spine_msg_args_t;
