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
  register long a7 asm("a7") = 0x100; /* SYSCALL_NOTIF_DONE */
  asm volatile("ecall" : : "r"(a7) : "memory");
}

/* Filesystem syscalls */
static inline long sys_open(const char *path, long flags) {
  register long a0 asm("a0") = (long)path;
  register long a1 asm("a1") = flags;
  register long a7 asm("a7") = 0x200; /* SYSCALL_OPEN */
  asm volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a7) : "memory");
  return a0;
}

static inline long sys_read(long fd, void *buf, long n) {
  register long a0 asm("a0") = fd;
  register long a1 asm("a1") = (long)buf;
  register long a2 asm("a2") = n;
  register long a7 asm("a7") = 0x201; /* SYSCALL_READ */
  asm volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a2), "r"(a7) : "memory");
  return a0;
}

static inline long sys_write(long fd, const void *buf, long n) {
  register long a0 asm("a0") = fd;
  register long a1 asm("a1") = (long)buf;
  register long a2 asm("a2") = n;
  register long a7 asm("a7") = 0x205; /* SYSCALL_WRITE */
  asm volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a2), "r"(a7) : "memory");
  return a0;
}

static inline long sys_close(long fd) {
  register long a0 asm("a0") = fd;
  register long a7 asm("a7") = 0x202; /* SYSCALL_CLOSE */
  asm volatile("ecall" : "+r"(a0) : "r"(a7) : "memory");
  return a0;
}

typedef struct {
  unsigned long size;
  unsigned short mode;
  unsigned short type;
  unsigned int nlink;
} sys_stat_t;

static inline long sys_stat(const char *path, sys_stat_t *out) {
  register long a0 asm("a0") = (long)path;
  register long a1 asm("a1") = (long)out;
  register long a7 asm("a7") = 0x203; /* SYSCALL_STAT */
  asm volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a7) : "memory");
  return a0;
}

typedef struct __attribute__((packed)) {
  unsigned char namelen;
  unsigned char type;
  unsigned short _pad;
  char name[64];
} sys_dirent_t;

static inline long sys_getdents(const char *path, sys_dirent_t *buf, long cap) {
  register long a0 asm("a0") = (long)path;
  register long a1 asm("a1") = (long)buf;
  register long a2 asm("a2") = cap;
  register long a7 asm("a7") = 0x204; /* SYSCALL_GETDENTS */
  asm volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a2), "r"(a7) : "memory");
  return a0;
}

/* Named fork syscalls */
static inline long sys_fork_open(const char *base_path, const char *fork_name, long flags) {
  register long a0 asm("a0") = (long)base_path;
  register long a1 asm("a1") = (long)fork_name;
  register long a2 asm("a2") = flags;
  register long a7 asm("a7") = 0x210; /* SYSCALL_FORK_OPEN */
  asm volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a2), "r"(a7) : "memory");
  return a0;
}

static inline long sys_fork_list(const char *base_path, sys_dirent_t *buf, long cap) {
  register long a0 asm("a0") = (long)base_path;
  register long a1 asm("a1") = (long)buf;
  register long a2 asm("a2") = cap;
  register long a7 asm("a7") = 0x211; /* SYSCALL_FORK_LIST */
  asm volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a2), "r"(a7) : "memory");
  return a0;
}

static inline long sys_fork_stat(const char *base_path, const char *fork_name, sys_stat_t *st) {
  register long a0 asm("a0") = (long)base_path;
  register long a1 asm("a1") = (long)fork_name;
  register long a2 asm("a2") = (long)st;
  register long a7 asm("a7") = 0x212; /* SYSCALL_FORK_STAT */
  asm volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a2), "r"(a7) : "memory");
  return a0;
}
