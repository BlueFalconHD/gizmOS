#pragma once

#include <lib/types.h>
#include "process.h"

// Minimal legacy VFS stat/types to keep old syscalls compiling
typedef enum {
  VFS_NODE_UNKNOWN = 0,
  VFS_NODE_FILE = 1,
  VFS_NODE_DIR = 2,
  VFS_NODE_SYMLINK = 3,
} vfs_node_kind_t;

typedef struct vfs_stat {
  uint64_t size;
  uint16_t mode;
  uint16_t type; /* vfs_node_kind_t */
  uint32_t nlink;
} vfs_stat_t;

typedef struct fs_file {
  uint64_t    obj_id;  /* reserved for future */
  uint64_t    offset;
  uint32_t    flags;
  uint32_t    kind;   /* 0=vnode, 1=console.out */
} fs_file_t;

enum {
  FS_FKIND_VNODE = 0,
  FS_FKIND_STDIN = 1,
  FS_FKIND_STDOUT = 2,
  FS_FKIND_UART_OUT = 3,
};

/* Process FD table helpers */
int  fs_fd_alloc(proc_t *p, fs_file_t *f);
void fs_fd_close(proc_t *p, int fd);
void fs_close_all(proc_t *p);

/* Path operations */
int  fs_open_path(proc_t *p, const char *kpath, uint32_t flags);
int  fs_stat_path(proc_t *p, const char *kpath, vfs_stat_t *out);
int  fs_listdir_path(proc_t *p, const char *kpath,
                     void *user_buf, uint64_t user_cap, uint64_t *out_bytes);

/* IO */
int  fs_read_fd(proc_t *p, int fd, uint64_t user_dst, uint64_t nbytes);
int  fs_write_fd(proc_t *p, int fd, uint64_t user_src, uint64_t nbytes);

/* Named fork helpers (build "/..namedfork/<name>" under base path) */
int  fs_open_named_fork(proc_t *p, const char *base_path, const char *fork_name, uint32_t flags);
int  fs_list_named_forks(proc_t *p, const char *base_path,
                         void *user_buf, uint64_t user_cap, uint64_t *out_bytes);
int  fs_stat_named_fork(proc_t *p, const char *base_path, const char *fork_name, vfs_stat_t *out);

/* Standard FDs for new processes: stdout=Console.out, stderr=Uart.out */
void fs_install_standard_fds(proc_t *p);


