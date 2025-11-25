#pragma once

#include <lib/types.h>
#include <stdint.h>

struct proc;

#define SPINE_SERVICE_NAME_MAX 16

typedef struct {
  uint64_t token;
  uint32_t pid;
  char     name[16];
  char     service[16];
} spine_seal_t;

// Header that prefixes every Spine message delivered via notifications.
// Followed by `message_size` bytes of payload.
typedef struct __attribute__((packed)) {
  uint64_t sender_token;
  uint32_t message_size;
  uint32_t reserved;
} spine_wire_msg_t;

void   spine_init_proc(struct proc *p);
void   spine_on_exit(struct proc *p);

g_bool spine_service_advertise(struct proc *p, const char *name, uint32_t flags);
// Returns PID on success, or -1 if not found.
int64_t spine_service_lookup(const char *name, uint32_t flags);

// user_src points at user VA in src's address space.
g_bool spine_msg_send(struct proc *src, int dest_pid,
                      const void *user_src, uint64_t size, uint32_t flags);

// Fill out seal info for a given token. Returns false if not found.
g_bool spine_get_seal(uint64_t token, spine_seal_t *out);


