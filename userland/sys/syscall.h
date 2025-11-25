#pragma once

#include <stdint.h>

static inline long sys_print_int(long v) {
  register long a0 asm("a0") = v;
  register long a7 asm("a7") = 0x10; /* SYSCALL_PRINT_INT */
  asm volatile("ecall" : "+r"(a0) : "r"(a7) : "memory");
  return a0;
}

static inline long sys_print_str(const char *str) {
  register long a0 asm("a0") = (long)str;
  register long a7 asm("a7") = 0x11; /* SYSCALL_PRINT_STR */
  asm volatile("ecall" : "+r"(a0) : "r"(a7) : "memory");
  return a0;
}

static inline void sys_exit(int status) {
  register long a0 asm("a0") = status;
  register long a7 asm("a7") = 0x02; /* SYSCALL_EXIT */
  asm volatile("ecall" : : "r"(a0), "r"(a7) : "memory");
  __builtin_unreachable();
}

static inline long sys_spawn(const char *path, const char *name) {
  register long a0 asm("a0") = (long)path;
  register long a1 asm("a1") = (long)name;
  register long a7 asm("a7") = 0x03; /* SYSCALL_SPAWN */
  asm volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a7) : "memory");
  return a0;
}

static inline long sys_wait(long *status_out) {
  register long a0 asm("a0") = (long)status_out;
  register long a7 asm("a7") = 0x04; /* SYSCALL_WAIT */
  asm volatile("ecall" : "+r"(a0) : "r"(a7) : "memory");
  return a0;
}

static inline uint32_t sys_notif_register(uint16_t type, uint64_t handler,
                                          uint64_t arg, uint32_t flags) {
  register long a0 asm("a0") = (long)type;
  register long a1 asm("a1") = (long)handler;
  register long a2 asm("a2") = (long)arg;
  register long a3 asm("a3") = (long)flags;
  register long a7 asm("a7") = 0x90; /* SYSCALL_NOTIF_REGISTER */
  asm volatile("ecall"
               : "+r"(a0)
               : "r"(a1), "r"(a2), "r"(a3), "r"(a7)
               : "memory");
  return (uint32_t)a0;
}

static inline int sys_notif_unregister(uint16_t type, uint32_t id) {
  register long a0 asm("a0") = (long)type;
  register long a1 asm("a1") = (long)id;
  register long a7 asm("a7") = 0x91; /* SYSCALL_NOTIF_UNREGISTER */
  asm volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a7) : "memory");
  return (int)a0;
}

static inline void sys_notif_done() {
  /* Match other syscall wrappers: set a7 and issue ecall. */
  register long a0 asm("a0") = 0;
  register long a7 asm("a7") = 0x100; /* SYSCALL_NOTIF_DONE */
  asm volatile("ecall" : "+r"(a0) : "r"(a7) : "memory");
}

/* Spine IPC */
typedef struct {
  uint64_t token;
  uint32_t pid;
  char     name[16];
  char     service[16];
} sys_spine_seal_t;

static inline long sys_spine_msg_send(long dest_pid, const void *buf, long n, unsigned long flags) {
  register long a0 asm("a0") = dest_pid;
  register long a1 asm("a1") = (long)buf;
  register long a2 asm("a2") = n;
  register long a3 asm("a3") = (long)flags;
  register long a7 asm("a7") = 0x180;
  asm volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a2), "r"(a3), "r"(a7) : "memory");
  return a0;
}

static inline long sys_spine_service_advertise(const char *name, unsigned long flags) {
  register long a0 asm("a0") = (long)name;
  register long a1 asm("a1") = (long)flags;
  register long a7 asm("a7") = 0x181;
  asm volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a7) : "memory");
  return a0;
}

// Returns pid on success, or -1 on failure/not found.
static inline long sys_spine_service_lookup(const char *name, unsigned long flags) {
  register long a0 asm("a0") = (long)name;
  register long a1 asm("a1") = (long)flags;
  register long a7 asm("a7") = 0x182;
  asm volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a7) : "memory");
  return a0;
}

static inline long sys_spine_get_seal(uint64_t token, void *out, long out_size) {
  register long a0 asm("a0") = (long)token;
  register long a1 asm("a1") = (long)out;
  register long a2 asm("a2") = (long)out_size;
  register long a7 asm("a7") = 0x183;
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
  register long a7 asm("a7") = 0x220;
  asm volatile("ecall" : "+r"(a0) : "r"(a7) : "memory");
  return a0;
}
static inline long sys_obj_stat(long obj_id, sys_obj_stat_t *out) {
  register long a0 asm("a0") = obj_id;
  register long a1 asm("a1") = (long)out;
  register long a7 asm("a7") = 0x221;
  asm volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a7) : "memory");
  return a0;
}
static inline long sys_obj_list_subobjects(long obj_id, sys_objdirent_t *buf, long cap) {
  register long a0 asm("a0") = obj_id;
  register long a1 asm("a1") = (long)buf;
  register long a2 asm("a2") = cap;
  register long a7 asm("a7") = 0x222;
  asm volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a2), "r"(a7) : "memory");
  return a0;
}
static inline long sys_obj_read(long obj_id, void *dst, unsigned long offset, long n) {
  register long a0 asm("a0") = obj_id;
  register long a1 asm("a1") = (long)dst;
  register long a2 asm("a2") = (long)offset;
  register long a3 asm("a3") = n;
  register long a7 asm("a7") = 0x223;
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
  register long a7 asm("a7") = 0x224;
  asm volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a2), "r"(a3), "r"(a4), "r"(a7) : "memory");
  return a0;
}
static inline long sys_obj_attr_list(long obj_id, sys_objattr_t *buf, long cap) {
  register long a0 asm("a0") = obj_id;
  register long a1 asm("a1") = (long)buf;
  register long a2 asm("a2") = cap;
  register long a7 asm("a7") = 0x225;
  asm volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a2), "r"(a7) : "memory");
  return a0;
}
static inline long sys_obj_desc(long obj_id, sys_obj_desc_t *out) {
  register long a0 asm("a0") = obj_id;
  register long a1 asm("a1") = (long)out;
  register long a7 asm("a7") = 0x226;
  asm volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a7) : "memory");
  return a0;
}

/* Object handle API */
static inline long sys_objh_id_at(const char *path) {
  register long a0 asm("a0") = (long)path;
  register long a7 asm("a7") = 0x240;
  asm volatile("ecall" : "+r"(a0) : "r"(a7) : "memory");
  return a0;
}
static inline long sys_objh_open(long obj_id, unsigned long flags) {
  register long a0 asm("a0") = obj_id;
  register long a1 asm("a1") = (long)flags;
  register long a7 asm("a7") = 0x241;
  asm volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a7) : "memory");
  return a0;
}
static inline long sys_objh_close(long handle) {
  register long a0 asm("a0") = handle;
  register long a7 asm("a7") = 0x242;
  asm volatile("ecall" : "+r"(a0) : "r"(a7) : "memory");
  return a0;
}
static inline long sys_objh_open_at(const char *path, unsigned long flags) {
  register long a0 asm("a0") = (long)path;
  register long a1 asm("a1") = (long)flags;
  register long a7 asm("a7") = 0x246;
  asm volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a7) : "memory");
  return a0;
}
static inline long sys_objh_stat(long handle, sys_obj_stat_t *out) {
  register long a0 asm("a0") = handle;
  register long a1 asm("a1") = (long)out;
  register long a7 asm("a7") = 0x247;
  asm volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a7) : "memory");
  return a0;
}
static inline long sys_objh_list_subobjects(long handle, sys_objdirent_t *buf, long cap) {
  register long a0 asm("a0") = handle;
  register long a1 asm("a1") = (long)buf;
  register long a2 asm("a2") = cap;
  register long a7 asm("a7") = 0x248;
  asm volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a2), "r"(a7) : "memory");
  return a0;
}
static inline long sys_objh_read(long handle, void *dst, unsigned long offset, long n) {
  register long a0 asm("a0") = handle;
  register long a1 asm("a1") = (long)dst;
  register long a2 asm("a2") = (long)offset;
  register long a3 asm("a3") = n;
  register long a7 asm("a7") = 0x249;
  asm volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a2), "r"(a3), "r"(a7) : "memory");
  return a0;
}
static inline long sys_objh_write(long handle, const void *src, unsigned long offset, long n) {
  register long a0 asm("a0") = handle;
  register long a1 asm("a1") = (long)src;
  register long a2 asm("a2") = (long)offset;
  register long a3 asm("a3") = n;
  register long a7 asm("a7") = 0x24A;
  asm volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a2), "r"(a3), "r"(a7) : "memory");
  return a0;
}

static inline long sys_objh_seek(long handle, long off, long whence) {
  register long a0 asm("a0") = handle;
  register long a1 asm("a1") = off;
  register long a2 asm("a2") = whence;
  register long a7 asm("a7") = 0x34A; /* matches SYSCALL_NUM_OBJH_SEEK */
  asm volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a2), "r"(a7) : "memory");
  return a0;
}
static inline long sys_objh_attr_get(long handle, const char *key, void *out_value_struct, char *strbuf, long cap) {
  register long a0 asm("a0") = handle;
  register long a1 asm("a1") = (long)key;
  register long a2 asm("a2") = (long)out_value_struct;
  register long a3 asm("a3") = (long)strbuf;
  register long a4 asm("a4") = cap;
  register long a7 asm("a7") = 0x24B;
  asm volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a2), "r"(a3), "r"(a4), "r"(a7) : "memory");
  return a0;
}
static inline long sys_objh_attr_list(long handle, void *buf, long cap) {
  register long a0 asm("a0") = handle;
  register long a1 asm("a1") = (long)buf;
  register long a2 asm("a2") = cap;
  register long a7 asm("a7") = 0x24C;
  asm volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a2), "r"(a7) : "memory");
  return a0;
}
static inline long sys_objh_desc(long handle, sys_obj_desc_t *out) {
  register long a0 asm("a0") = handle;
  register long a1 asm("a1") = (long)out;
  register long a7 asm("a7") = 0x24D;
  asm volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a7) : "memory");
  return a0;
}

/* No separate get_id syscall; use sys_objh_desc and read .id */
static inline long sys_objh_create(long parent_handle, const char *name, unsigned long mode, unsigned long kind) {
  register long a0 asm("a0") = parent_handle;
  register long a1 asm("a1") = (long)name;
  register long a2 asm("a2") = (long)mode;
  register long a3 asm("a3") = (long)kind;
  register long a7 asm("a7") = 0x24E;
  asm volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a2), "r"(a3), "r"(a7) : "memory");
  return a0;
}
static inline long sys_objh_set_attr(long handle, const char *key, unsigned long type, const void *value, long cap_or_size) {
  register long a0 asm("a0") = handle;
  register long a1 asm("a1") = (long)key;
  register long a2 asm("a2") = (long)type;
  register long a3 asm("a3") = (long)value;
  register long a4 asm("a4") = cap_or_size;
  register long a7 asm("a7") = 0x24F;
  asm volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a2), "r"(a3), "r"(a4), "r"(a7) : "memory");
  return a0;
}
static inline long sys_objh_link(long parent_handle, const char *name, long target_handle) {
  register long a0 asm("a0") = parent_handle;
  register long a1 asm("a1") = (long)name;
  register long a2 asm("a2") = target_handle;
  register long a7 asm("a7") = 0x250;
  asm volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a2), "r"(a7) : "memory");
  return a0;
}
static inline long sys_objh_unlink(long parent_handle, const char *name) {
  register long a0 asm("a0") = parent_handle;
  register long a1 asm("a1") = (long)name;
  register long a7 asm("a7") = 0x251;
  asm volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a7) : "memory");
  return a0;
}
static inline long sys_objh_rename(long parent_handle, const char *old_name, const char *new_name) {
  register long a0 asm("a0") = parent_handle;
  register long a1 asm("a1") = (long)old_name;
  register long a2 asm("a2") = (long)new_name;
  register long a7 asm("a7") = 0x252;
  asm volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a2), "r"(a7) : "memory");
  return a0;
}
static inline long sys_objh_has_subs(long handle) {
  register long a0 asm("a0") = handle;
  register long a7 asm("a7") = 0x243;
  asm volatile("ecall" : "+r"(a0) : "r"(a7) : "memory");
  return a0;
}
static inline long sys_objh_subs_count(long handle) {
  register long a0 asm("a0") = handle;
  register long a7 asm("a7") = 0x244;
  asm volatile("ecall" : "+r"(a0) : "r"(a7) : "memory");
  return a0;
}
static inline long sys_objh_sub_at(long handle, unsigned long index) {
  register long a0 asm("a0") = handle;
  register long a1 asm("a1") = (long)index;
  register long a7 asm("a7") = 0x245;
  asm volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a7) : "memory");
  return a0;
}

/* Mutation (legacy, discouraged: prefer handle-based ops) */
static inline long sys_obj_create(long parent_id, const char *name, unsigned long mode, unsigned long kind) {
  register long a0 asm("a0") = parent_id;
  register long a1 asm("a1") = (long)name;
  register long a2 asm("a2") = (long)mode;
  register long a3 asm("a3") = (long)kind;
  register long a7 asm("a7") = 0x230;
  asm volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a2), "r"(a3), "r"(a7) : "memory");
  return a0;
}
static inline long sys_obj_write(long obj_id, const void *src, unsigned long offset, long n) {
  register long a0 asm("a0") = obj_id;
  register long a1 asm("a1") = (long)src;
  register long a2 asm("a2") = (long)offset;
  register long a3 asm("a3") = n;
  register long a7 asm("a7") = 0x231;
  asm volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a2), "r"(a3), "r"(a7) : "memory");
  return a0;
}
static inline long sys_obj_set_attr(long obj_id, const char *key, unsigned long type, const void *value, long cap_or_size) {
  register long a0 asm("a0") = obj_id;
  register long a1 asm("a1") = (long)key;
  register long a2 asm("a2") = (long)type;
  register long a3 asm("a3") = (long)value;
  register long a4 asm("a4") = cap_or_size;
  register long a7 asm("a7") = 0x232;
  asm volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a2), "r"(a3), "r"(a4), "r"(a7) : "memory");
  return a0;
}
static inline long sys_obj_link(long parent_id, const char *name, unsigned long target_id) {
  register long a0 asm("a0") = parent_id;
  register long a1 asm("a1") = (long)name;
  register long a2 asm("a2") = (long)target_id;
  register long a7 asm("a7") = 0x233;
  asm volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a2), "r"(a7) : "memory");
  return a0;
}
static inline long sys_obj_unlink(long parent_id, const char *name) {
  register long a0 asm("a0") = parent_id;
  register long a1 asm("a1") = (long)name;
  register long a7 asm("a7") = 0x234;
  asm volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a7) : "memory");
  return a0;
}
static inline long sys_obj_rename(long parent_id, const char *old_name, const char *new_name) {
  register long a0 asm("a0") = parent_id;
  register long a1 asm("a1") = (long)old_name;
  register long a2 asm("a2") = (long)new_name;
  register long a7 asm("a7") = 0x235;
  asm volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a2), "r"(a7) : "memory");
  return a0;
}
