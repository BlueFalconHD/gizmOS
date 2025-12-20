#pragma once

#include <stdint.h>
#include "../../include/syscall_numbers.h"
#include "../../include/spine_ipc.h"

static inline long sys_print_int(long v) {
  register long a0 asm("a0") = v;
  register long a7 asm("a7") = SYSNO_PRINT_INT; /* SYSCALL_PRINT_INT */
  asm volatile("ecall" : "+r"(a0) : "r"(a7) : "memory");
  return a0;
}

static inline long sys_print_str(const char *str) {
  register long a0 asm("a0") = (long)str;
  register long a7 asm("a7") = SYSNO_PRINT_STR; /* SYSCALL_PRINT_STR */
  asm volatile("ecall" : "+r"(a0) : "r"(a7) : "memory");
  return a0;
}

static inline void *sys_sbrk(long increment) {
  register long a0 asm("a0") = increment;
  register long a7 asm("a7") = SYSNO_SBRK; /* SYSCALL_SBRK */
  asm volatile("ecall" : "+r"(a0) : "r"(a7) : "memory");
  return (void *)a0;
}

static inline void sys_exit(int status) {
  register long a0 asm("a0") = status;
  register long a7 asm("a7") = SYSNO_EXIT; /* SYSCALL_EXIT */
  asm volatile("ecall" : : "r"(a0), "r"(a7) : "memory");
  __builtin_unreachable();
}

static inline long sys_spawn(const char *path, const char *name) {
  register long a0 asm("a0") = (long)path;
  register long a1 asm("a1") = (long)name;
  register long a7 asm("a7") = SYSNO_SPAWN; /* SYSCALL_SPAWN */
  asm volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a7) : "memory");
  return a0;
}

static inline long sys_wait(long *status_out) {
  register long a0 asm("a0") = (long)status_out;
  register long a7 asm("a7") = SYSNO_WAIT; /* SYSCALL_WAIT */
  asm volatile("ecall" : "+r"(a0) : "r"(a7) : "memory");
  return a0;
}

// Threads
static inline long sys_thread_create(uint64_t entry_va, uint64_t arg, void *stack_top) {
  register long a0 asm("a0") = (long)entry_va;
  register long a1 asm("a1") = (long)arg;
  register long a2 asm("a2") = (long)stack_top;
  register long a7 asm("a7") = SYSNO_THREAD_CREATE;
  asm volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a2), "r"(a7) : "memory");
  return a0;
}

// Returns tid on success, -1 on error.
static inline long sys_thread_join(long tid, long *status_out) {
  register long a0 asm("a0") = (long)tid;
  register long a1 asm("a1") = (long)status_out;
  register long a7 asm("a7") = SYSNO_THREAD_JOIN;
  asm volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a7) : "memory");
  return a0;
}

static inline void sys_thread_exit(long status) {
  register long a0 asm("a0") = (long)status;
  register long a7 asm("a7") = SYSNO_THREAD_EXIT;
  asm volatile("ecall" : : "r"(a0), "r"(a7) : "memory");
  __builtin_unreachable();
}

// Spawn with argv: path, name(optional), argv array, argc
static inline long sys_spawn2(const char *path, const char *name,
                              const char *const *argv, long argc) {
  register long a0 asm("a0") = (long)path;
  register long a1 asm("a1") = (long)name;
  register long a2 asm("a2") = (long)argv;
  register long a3 asm("a3") = (long)argc;
  register long a7 asm("a7") = SYSNO_SPAWN2;
  asm volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a2), "r"(a3), "r"(a7) : "memory");
  return a0;
}

static inline uint32_t sys_notif_register(uint16_t type, uint64_t handler,
                                          uint64_t arg, uint32_t flags) {
  register long a0 asm("a0") = (long)type;
  register long a1 asm("a1") = (long)handler;
  register long a2 asm("a2") = (long)arg;
  register long a3 asm("a3") = (long)flags;
  register long a7 asm("a7") = SYSNO_NOTIF_REGISTER; /* SYSCALL_NOTIF_REGISTER */
  asm volatile("ecall"
               : "+r"(a0)
               : "r"(a1), "r"(a2), "r"(a3), "r"(a7)
               : "memory");
  return (uint32_t)a0;
}

static inline int sys_notif_unregister(uint16_t type, uint32_t id) {
  register long a0 asm("a0") = (long)type;
  register long a1 asm("a1") = (long)id;
  register long a7 asm("a7") = SYSNO_NOTIF_UNREGISTER; /* SYSCALL_NOTIF_UNREGISTER */
  asm volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a7) : "memory");
  return (int)a0;
}

static inline void sys_notif_done() {
  /* Match other syscall wrappers: set a7 and issue ecall. */
  register long a0 asm("a0") = 0;
  register long a7 asm("a7") = SYSNO_NOTIF_DONE; /* SYSCALL_NOTIF_DONE */
  asm volatile("ecall" : "+r"(a0) : "r"(a7) : "memory");
}

/* Spine IPC */
typedef struct {
  uint64_t token;
  uint32_t pid;
  char     name[16];
  char     service[16];
} sys_spine_seal_t;

static inline long sys_spine_msg(spine_msg_args_t *args, unsigned long args_size) {
  register long a0 asm("a0") = (long)args;
  register long a1 asm("a1") = (long)args_size;
  register long a7 asm("a7") = SYSNO_SPINE_MSG;
  asm volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a7) : "memory");
  return a0;
}

static inline long sys_spine_msg_send(long dest_pid, const void *buf, long n, unsigned long flags) {
  spine_msg_args_t a = {
    .dest_pid = (int64_t)dest_pid,
    .flags = (uint32_t)(SPINE_MSGF_SEND | (uint32_t)flags),
    .send_buf = (uint64_t)(uintptr_t)buf,
    .send_len = (uint64_t)n,
    .recv_buf = 0,
    .recv_cap = 0,
    .recv_len_out = 0,
    .sender_token_out = 0,
    .timeout_ticks = 0,
  };
  return sys_spine_msg(&a, (unsigned long)sizeof(a));
}

// Receive a Spine message body into `buf` (payload only).
// Returns:
// - 0 on success (writes payload size to *out_len)
// - -2 on timeout
// - -3 if buffer too small (message left queued; *out_len is required size)
// - -1 on other failure
static inline long sys_spine_msg_recv(void *buf, long cap, long *out_len,
                                      uint64_t *out_sender_token,
                                      unsigned long timeout_ticks) {
  spine_msg_args_t a = {
    .dest_pid = -1,
    .flags = SPINE_MSGF_RECV | SPINE_MSGF_RECV_BODY_ONLY,
    .send_buf = 0,
    .send_len = 0,
    .recv_buf = (uint64_t)(uintptr_t)buf,
    .recv_cap = (uint64_t)cap,
    .recv_len_out = (uint64_t)(uintptr_t)out_len,
    .sender_token_out = (uint64_t)(uintptr_t)out_sender_token,
    .timeout_ticks = (uint64_t)timeout_ticks,
  };
  return sys_spine_msg(&a, (unsigned long)sizeof(a));
}

static inline long sys_spine_service_advertise(const char *name, unsigned long flags) {
  register long a0 asm("a0") = (long)name;
  register long a1 asm("a1") = (long)flags;
  register long a7 asm("a7") = SYSNO_SPINE_SERVICE_ADVERTISE;
  asm volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a7) : "memory");
  return a0;
}

// Returns pid on success, or -1 on failure/not found.
static inline long sys_spine_service_lookup(const char *name, unsigned long flags) {
  register long a0 asm("a0") = (long)name;
  register long a1 asm("a1") = (long)flags;
  register long a7 asm("a7") = SYSNO_SPINE_SERVICE_LOOKUP;
  asm volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a7) : "memory");
  return a0;
}

static inline long sys_spine_get_seal(uint64_t token, void *out, long out_size) {
  register long a0 asm("a0") = (long)token;
  register long a1 asm("a1") = (long)out;
  register long a2 asm("a2") = (long)out_size;
  register long a7 asm("a7") = SYSNO_SPINE_GET_SEAL;
  asm volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a2), "r"(a7) : "memory");
  return a0;
}

/* ObjectFS syscalls (object-centric) */
typedef struct {
  uint64_t size;
  uint16_t mode;
  uint8_t  kind;
  uint32_t nlink;
} sys_obj_stat_t;

typedef struct __attribute__((packed)) {
  uint8_t  name_len;
  uint8_t  type;
  uint16_t _pad;
  uint64_t id;
  char     name[64];
} sys_objdirent_t;

typedef struct __attribute__((packed)) {
  uint8_t  key_len;
  uint8_t  type; /* 0=str, 1=int, 2=bool */
  uint16_t _pad;
  char     key[64];
} sys_objattr_t;

typedef struct __attribute__((packed)) {
  uint64_t id;
  uint16_t mode;
  uint8_t  kind;
  uint8_t  flags;
  uint32_t uid;
  uint32_t gid;
  uint32_t nlink;
  uint64_t size;
  uint64_t atime, mtime, ctime;
  uint64_t target_id;
} sys_obj_desc_t;

/* Basic object resolution and metadata */
static inline long sys_obj_id_at(const char *path) {
  register long a0 asm("a0") = (long)path;
  register long a7 asm("a7") = SYSNO_OBJ_LOOKUP_PATH;
  asm volatile("ecall" : "+r"(a0) : "r"(a7) : "memory");
  return a0;
}
static inline long sys_obj_stat(long obj_id, sys_obj_stat_t *out) {
  register long a0 asm("a0") = obj_id;
  register long a1 asm("a1") = (long)out;
  register long a7 asm("a7") = SYSNO_OBJ_STAT;
  asm volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a7) : "memory");
  return a0;
}
static inline long sys_obj_list_subobjects(long obj_id, sys_objdirent_t *buf, long cap) {
  register long a0 asm("a0") = obj_id;
  register long a1 asm("a1") = (long)buf;
  register long a2 asm("a2") = cap;
  register long a7 asm("a7") = SYSNO_OBJ_LIST_SUBOBJECTS;
  asm volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a2), "r"(a7) : "memory");
  return a0;
}
static inline long sys_obj_read(long obj_id, void *dst, unsigned long offset, long n) {
  register long a0 asm("a0") = obj_id;
  register long a1 asm("a1") = (long)dst;
  register long a2 asm("a2") = (long)offset;
  register long a3 asm("a3") = n;
  register long a7 asm("a7") = SYSNO_OBJ_READ;
  asm volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a2), "r"(a3), "r"(a7) : "memory");
  return a0;
}
/* Returns 0 for non-string attributes; for strings returns length; or -1 on error */
static inline long sys_obj_attr_get(long obj_id, const char *key, void *out_value_struct, char *strbuf, long cap) {
  register long a0 asm("a0") = obj_id;
  register long a1 asm("a1") = (long)key;
  register long a2 asm("a2") = (long)out_value_struct;
  register long a3 asm("a3") = (long)strbuf;
  register long a4 asm("a4") = cap;
  register long a7 asm("a7") = SYSNO_OBJ_ATTR_GET;
  asm volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a2), "r"(a3), "r"(a4), "r"(a7) : "memory");
  return a0;
}
static inline long sys_obj_attr_list(long obj_id, sys_objattr_t *buf, long cap) {
  register long a0 asm("a0") = obj_id;
  register long a1 asm("a1") = (long)buf;
  register long a2 asm("a2") = cap;
  register long a7 asm("a7") = SYSNO_OBJ_ATTR_LIST;
  asm volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a2), "r"(a7) : "memory");
  return a0;
}
static inline long sys_obj_desc(long obj_id, sys_obj_desc_t *out) {
  register long a0 asm("a0") = obj_id;
  register long a1 asm("a1") = (long)out;
  register long a7 asm("a7") = SYSNO_OBJ_DESC;
  asm volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a7) : "memory");
  return a0;
}

/* Object handle API */
static inline long sys_objh_id_at(const char *path) {
  register long a0 asm("a0") = (long)path;
  register long a7 asm("a7") = SYSNO_OBJH_ID_AT;
  asm volatile("ecall" : "+r"(a0) : "r"(a7) : "memory");
  return a0;
}
static inline long sys_objh_open(long obj_id, unsigned long flags) {
  register long a0 asm("a0") = obj_id;
  register long a1 asm("a1") = (long)flags;
  register long a7 asm("a7") = SYSNO_OBJH_OPEN;
  asm volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a7) : "memory");
  return a0;
}
static inline long sys_objh_close(long handle) {
  register long a0 asm("a0") = handle;
  register long a7 asm("a7") = SYSNO_OBJH_CLOSE;
  asm volatile("ecall" : "+r"(a0) : "r"(a7) : "memory");
  return a0;
}
static inline long sys_objh_open_at(const char *path, unsigned long flags) {
  register long a0 asm("a0") = (long)path;
  register long a1 asm("a1") = (long)flags;
  register long a7 asm("a7") = SYSNO_OBJH_OPEN_AT;
  asm volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a7) : "memory");
  return a0;
}
static inline long sys_objh_stat(long handle, sys_obj_stat_t *out) {
  register long a0 asm("a0") = handle;
  register long a1 asm("a1") = (long)out;
  register long a7 asm("a7") = SYSNO_OBJH_STAT;
  asm volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a7) : "memory");
  return a0;
}
static inline long sys_objh_list_subobjects(long handle, sys_objdirent_t *buf, long cap) {
  register long a0 asm("a0") = handle;
  register long a1 asm("a1") = (long)buf;
  register long a2 asm("a2") = cap;
  register long a7 asm("a7") = SYSNO_OBJH_LIST_SUBOBJECTS;
  asm volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a2), "r"(a7) : "memory");
  return a0;
}
static inline long sys_objh_read(long handle, void *dst, unsigned long offset, long n) {
  register long a0 asm("a0") = handle;
  register long a1 asm("a1") = (long)dst;
  register long a2 asm("a2") = (long)offset;
  register long a3 asm("a3") = n;
  register long a7 asm("a7") = SYSNO_OBJH_READ;
  asm volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a2), "r"(a3), "r"(a7) : "memory");
  return a0;
}
static inline long sys_objh_write(long handle, const void *src, unsigned long offset, long n) {
  register long a0 asm("a0") = handle;
  register long a1 asm("a1") = (long)src;
  register long a2 asm("a2") = (long)offset;
  register long a3 asm("a3") = n;
  register long a7 asm("a7") = SYSNO_OBJH_WRITE;
  asm volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a2), "r"(a3), "r"(a7) : "memory");
  return a0;
}

static inline long sys_objh_seek(long handle, long off, long whence) {
  register long a0 asm("a0") = handle;
  register long a1 asm("a1") = off;
  register long a2 asm("a2") = whence;
  register long a7 asm("a7") = SYSNO_OBJH_SEEK; /* matches SYSCALL_NUM_OBJH_SEEK */
  asm volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a2), "r"(a7) : "memory");
  return a0;
}
static inline long sys_objh_attr_get(long handle, const char *key, void *out_value_struct, char *strbuf, long cap) {
  register long a0 asm("a0") = handle;
  register long a1 asm("a1") = (long)key;
  register long a2 asm("a2") = (long)out_value_struct;
  register long a3 asm("a3") = (long)strbuf;
  register long a4 asm("a4") = cap;
  register long a7 asm("a7") = SYSNO_OBJH_ATTR_GET;
  asm volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a2), "r"(a3), "r"(a4), "r"(a7) : "memory");
  return a0;
}
static inline long sys_objh_attr_list(long handle, void *buf, long cap) {
  register long a0 asm("a0") = handle;
  register long a1 asm("a1") = (long)buf;
  register long a2 asm("a2") = cap;
  register long a7 asm("a7") = SYSNO_OBJH_ATTR_LIST;
  asm volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a2), "r"(a7) : "memory");
  return a0;
}
static inline long sys_objh_desc(long handle, sys_obj_desc_t *out) {
  register long a0 asm("a0") = handle;
  register long a1 asm("a1") = (long)out;
  register long a7 asm("a7") = SYSNO_OBJH_DESC;
  asm volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a7) : "memory");
  return a0;
}

/* No separate get_id syscall; use sys_objh_desc and read .id */
static inline long sys_objh_create(long parent_handle, const char *name, unsigned long mode, unsigned long kind) {
  register long a0 asm("a0") = parent_handle;
  register long a1 asm("a1") = (long)name;
  register long a2 asm("a2") = (long)mode;
  register long a3 asm("a3") = (long)kind;
  register long a7 asm("a7") = SYSNO_OBJH_CREATE;
  asm volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a2), "r"(a3), "r"(a7) : "memory");
  return a0;
}
static inline long sys_objh_set_attr(long handle, const char *key, unsigned long type, const void *value, long cap_or_size) {
  register long a0 asm("a0") = handle;
  register long a1 asm("a1") = (long)key;
  register long a2 asm("a2") = (long)type;
  register long a3 asm("a3") = (long)value;
  register long a4 asm("a4") = cap_or_size;
  register long a7 asm("a7") = SYSNO_OBJH_SET_ATTR;
  asm volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a2), "r"(a3), "r"(a4), "r"(a7) : "memory");
  return a0;
}
static inline long sys_objh_link(long parent_handle, const char *name, long target_handle) {
  register long a0 asm("a0") = parent_handle;
  register long a1 asm("a1") = (long)name;
  register long a2 asm("a2") = target_handle;
  register long a7 asm("a7") = SYSNO_OBJH_LINK;
  asm volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a2), "r"(a7) : "memory");
  return a0;
}
static inline long sys_objh_unlink(long parent_handle, const char *name) {
  register long a0 asm("a0") = parent_handle;
  register long a1 asm("a1") = (long)name;
  register long a7 asm("a7") = SYSNO_OBJH_UNLINK;
  asm volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a7) : "memory");
  return a0;
}
static inline long sys_objh_rename(long parent_handle, const char *old_name, const char *new_name) {
  register long a0 asm("a0") = parent_handle;
  register long a1 asm("a1") = (long)old_name;
  register long a2 asm("a2") = (long)new_name;
  register long a7 asm("a7") = SYSNO_OBJH_RENAME;
  asm volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a2), "r"(a7) : "memory");
  return a0;
}
static inline long sys_objh_has_subs(long handle) {
  register long a0 asm("a0") = handle;
  register long a7 asm("a7") = SYSNO_OBJH_HAS_SUBS;
  asm volatile("ecall" : "+r"(a0) : "r"(a7) : "memory");
  return a0;
}
static inline long sys_objh_subs_count(long handle) {
  register long a0 asm("a0") = handle;
  register long a7 asm("a7") = SYSNO_OBJH_SUBS_COUNT;
  asm volatile("ecall" : "+r"(a0) : "r"(a7) : "memory");
  return a0;
}
static inline long sys_objh_sub_at(long handle, unsigned long index) {
  register long a0 asm("a0") = handle;
  register long a1 asm("a1") = (long)index;
  register long a7 asm("a7") = SYSNO_OBJH_SUB_AT;
  asm volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a7) : "memory");
  return a0;
}

/* Mutation (legacy, discouraged: prefer handle-based ops) */
static inline long sys_obj_create(long parent_id, const char *name, unsigned long mode, unsigned long kind) {
  register long a0 asm("a0") = parent_id;
  register long a1 asm("a1") = (long)name;
  register long a2 asm("a2") = (long)mode;
  register long a3 asm("a3") = (long)kind;
  register long a7 asm("a7") = SYSNO_OBJ_CREATE;
  asm volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a2), "r"(a3), "r"(a7) : "memory");
  return a0;
}
static inline long sys_obj_write(long obj_id, const void *src, unsigned long offset, long n) {
  register long a0 asm("a0") = obj_id;
  register long a1 asm("a1") = (long)src;
  register long a2 asm("a2") = (long)offset;
  register long a3 asm("a3") = n;
  register long a7 asm("a7") = SYSNO_OBJ_WRITE;
  asm volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a2), "r"(a3), "r"(a7) : "memory");
  return a0;
}
static inline long sys_obj_set_attr(long obj_id, const char *key, unsigned long type, const void *value, long cap_or_size) {
  register long a0 asm("a0") = obj_id;
  register long a1 asm("a1") = (long)key;
  register long a2 asm("a2") = (long)type;
  register long a3 asm("a3") = (long)value;
  register long a4 asm("a4") = cap_or_size;
  register long a7 asm("a7") = SYSNO_OBJ_SET_ATTR;
  asm volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a2), "r"(a3), "r"(a4), "r"(a7) : "memory");
  return a0;
}
static inline long sys_obj_link(long parent_id, const char *name, unsigned long target_id) {
  register long a0 asm("a0") = parent_id;
  register long a1 asm("a1") = (long)name;
  register long a2 asm("a2") = (long)target_id;
  register long a7 asm("a7") = SYSNO_OBJ_LINK;
  asm volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a2), "r"(a7) : "memory");
  return a0;
}
static inline long sys_obj_unlink(long parent_id, const char *name) {
  register long a0 asm("a0") = parent_id;
  register long a1 asm("a1") = (long)name;
  register long a7 asm("a7") = SYSNO_OBJ_UNLINK;
  asm volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a7) : "memory");
  return a0;
}
static inline long sys_obj_rename(long parent_id, const char *old_name, const char *new_name) {
  register long a0 asm("a0") = parent_id;
  register long a1 asm("a1") = (long)old_name;
  register long a2 asm("a2") = (long)new_name;
  register long a7 asm("a7") = SYSNO_OBJ_RENAME;
  asm volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a2), "r"(a7) : "memory");
  return a0;
}
