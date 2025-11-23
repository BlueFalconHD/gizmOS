#include "fs.h"
#include <lib/kalloc.h>
#include <lib/print.h>
#include <lib/str.h>
#include <lib/usermem.h>

int fs_fd_alloc(struct proc *p, fs_file_t *f) {
  for (int i = 0; i < PROC_MAX_FD; i++) {
    if (p->fd_table[i] == NULL) {
      p->fd_table[i] = f;
      return i;
    }
  }
  return -1;
}

void fs_fd_close(struct proc *p, int fd) {
  if (!p || fd < 0 || fd >= PROC_MAX_FD)
    return;
  fs_file_t *f = p->fd_table[fd];
  if (!f)
    return;
  // vfs has no open/close; free wrapper
  kfree(f);
  p->fd_table[fd] = NULL;
}

void fs_close_all(struct proc *p) {
  for (int i = 0; i < PROC_MAX_FD; i++)
    fs_fd_close(p, i);
}

int fs_open_path(struct proc *p, const char *kpath, uint32_t flags) {
  (void)flags;
  /* Synthetic device outputs */
  if (!strcmp(kpath, "/Devices/Console.out")) {
    fs_file_t *f = (fs_file_t *)kalloc(sizeof(fs_file_t));
    if (!f)
      return -1;
    f->obj_id = 0;
    f->offset = 0;
    f->flags = flags;
    f->kind = FS_FKIND_STDOUT;
    int fd = fs_fd_alloc(p, f);
    if (fd < 0) {
      kfree(f);
      return -1;
    }
    return fd;
  }
  if (!strcmp(kpath, "/Devices/Uart.out")) {
    fs_file_t *f = (fs_file_t *)kalloc(sizeof(fs_file_t));
    if (!f)
      return -1;
    f->obj_id = 0;
    f->offset = 0;
    f->flags = flags;
    f->kind = FS_FKIND_UART_OUT;
    int fd = fs_fd_alloc(p, f);
    if (fd < 0) {
      kfree(f);
      return -1;
    }
    return fd;
  }
  // Legacy FS paths are no longer supported (ObjectFS uses new syscalls)
  return -1;
}

int fs_stat_path(struct proc *p, const char *kpath, vfs_stat_t *out) {
  (void)p;
  if (!strcmp(kpath, "/Devices/Console.out")) {
    out->size = 0;
    out->mode = 0666;
    out->type = VFS_NODE_FILE;
    out->nlink = 1;
    return 0;
  }
  if (!strcmp(kpath, "/Devices/Uart.out")) {
    out->size = 0;
    out->mode = 0666;
    out->type = VFS_NODE_FILE;
    out->nlink = 1;
    return 0;
  }
  // Unsupported for legacy paths
  return -1;
}

typedef struct __attribute__((packed)) {
  uint8_t namelen;
  uint8_t type; /* vfs_node_kind_t */
  uint16_t _pad;
  char name[64];
} dirent_user_t;

typedef struct {
  void *user_buf;
  uint64_t cap;
  uint64_t wrote;
  struct proc *p;
} readdir_ctx_t;

static void readdir_emit_to_user(const char *name, uint32_t type, void *arg) {
  readdir_ctx_t *ctx = (readdir_ctx_t *)arg;
  if (!ctx || !name)
    return;
  dirent_user_t de;
  uint64_t n = 0;
  while (name[n] && n < 64)
    n++;
  de.namelen = (uint8_t)n;
  de.type = (uint8_t)type;
  de._pad = 0;
  for (uint64_t i = 0; i < 64; i++)
    de.name[i] = (i < n) ? name[i] : '\0';

  if (ctx->wrote + sizeof(de) > ctx->cap)
    return;
  uint64_t dstva = (uint64_t)ctx->user_buf + ctx->wrote;
  if (result_is_ok(copyout(ctx->p->pagetable, dstva, &de, sizeof(de)))) {
    ctx->wrote += sizeof(de);
  }
}

int fs_listdir_path(struct proc *p, const char *kpath, void *user_buf,
                    uint64_t user_cap, uint64_t *out_bytes) {
  if (!strcmp(kpath, "/Devices")) {
    readdir_ctx_t ctx = {
        .user_buf = user_buf, .cap = user_cap, .wrote = 0, .p = p};
    // Emit Console.out
    readdir_emit_to_user("Console.out", VFS_NODE_FILE, &ctx);
    readdir_emit_to_user("Uart.out", VFS_NODE_FILE, &ctx);
    if (out_bytes)
      *out_bytes = ctx.wrote;
    return 0;
  }
  // Unsupported for legacy paths
  return -1;
}

int fs_read_fd(struct proc *p, int fd, uint64_t user_dst, uint64_t nbytes) {
  if (fd < 0 || fd >= PROC_MAX_FD)
    return -1;
  fs_file_t *f = p->fd_table[fd];
  if (!f || f->kind != FS_FKIND_VNODE)
    return -1;
  // No legacy file reading via path
  return -1;
}

int fs_write_fd(proc_t *p, int fd, uint64_t user_src, uint64_t nbytes) {
  if (fd < 0 || fd >= PROC_MAX_FD)
    return -1;
  fs_file_t *f = p->fd_table[fd];
  if (!f)
    return -1;
  if (f->kind == FS_FKIND_STDOUT) {
    char *kbuf = (char *)kalloc(nbytes + 1);
    if (!kbuf)
      return -1;
    if (!result_is_ok(copyin(p->pagetable, kbuf, user_src, nbytes))) {
      kfree(kbuf);
      return -1;
    }
    kbuf[nbytes] = '\0';
    print(kbuf, PRINT_FLAG_BOTH);
    kfree(kbuf);
    return (int)nbytes;
  }
  if (f->kind == FS_FKIND_UART_OUT) {
    char *kbuf = (char *)kalloc(nbytes + 1);
    if (!kbuf)
      return -1;
    if (!result_is_ok(copyin(p->pagetable, kbuf, user_src, nbytes))) {
      kfree(kbuf);
      return -1;
    }
    kbuf[nbytes] = '\0';
    print(kbuf, PRINT_FLAG_UART);
    kfree(kbuf);
    return (int)nbytes;
  }
  // not supported yet
  return -1;
}

static void build_namedfork_path(const char *base, const char *fork, char out[],
                                 uint64_t cap) {
  uint64_t bi = 0;
  uint64_t fi = 0;
  uint64_t oi = 0;
  while (base[bi] && oi + 1 < cap)
    out[oi++] = base[bi++];
  if (oi + 1 < cap)
    out[oi++] = '/';
  const char *nf = "..namedfork";
  while (*nf && oi + 1 < cap)
    out[oi++] = *nf++;
  if (oi + 1 < cap)
    out[oi++] = '/';
  while (fork[fi] && oi + 1 < cap)
    out[oi++] = fork[fi++];
  out[oi] = '\0';
}

int fs_open_named_fork(proc_t *p, const char *base_path, const char *fork_name,
                       uint32_t flags) {
  char path[256];
  build_namedfork_path(base_path, fork_name, path, sizeof(path));
  return fs_open_path(p, path, flags);
}

int fs_list_named_forks(proc_t *p, const char *base_path, void *user_buf,
                        uint64_t user_cap, uint64_t *out_bytes) {
  char dirpath[256];
  // list "/..namedfork" under base
  uint64_t bi = 0;
  uint64_t oi = 0;
  while (base_path[bi] && oi + 1 < sizeof(dirpath))
    dirpath[oi++] = base_path[bi++];
  if (oi + 1 < sizeof(dirpath))
    dirpath[oi++] = '/';
  const char *nf = "..namedfork";
  while (*nf && oi + 1 < sizeof(dirpath))
    dirpath[oi++] = *nf++;
  dirpath[oi] = '\0';
  return fs_listdir_path(p, dirpath, user_buf, user_cap, out_bytes);
}

int fs_stat_named_fork(proc_t *p, const char *base_path, const char *fork_name,
                       vfs_stat_t *out) {
  char path[256];
  build_namedfork_path(base_path, fork_name, path, sizeof(path));
  return fs_stat_path(p, path, out);
}

void fs_install_standard_fds(proc_t *p) {
  if (!p)
    return;
  // stdin (fd=0): placeholder synthetic (no read support yet)
  if (p->fd_table[0] == NULL) {
    fs_file_t *f = (fs_file_t *)kalloc(sizeof(fs_file_t));
    if (f) {
      f->obj_id = 0;
      f->offset = 0;
      f->flags = 0;
      f->kind = FS_FKIND_STDIN;
      p->fd_table[0] = f;
    }
  }
  // stdout (fd=1): Console.out via print()
  if (p->fd_table[1] == NULL) {
    fs_file_t *f = (fs_file_t *)kalloc(sizeof(fs_file_t));
    if (f) {
      f->obj_id = 0;
      f->offset = 0;
      f->flags = 0;
      f->kind = FS_FKIND_STDOUT;
      p->fd_table[1] = f;
    }
  }
  // stderr (fd=2): Uart.out
  if (p->fd_table[2] == NULL) {
    fs_file_t *f = (fs_file_t *)kalloc(sizeof(fs_file_t));
    if (f) {
      f->obj_id = 0;
      f->offset = 0;
      f->flags = 0;
      f->kind = FS_FKIND_UART_OUT;
      p->fd_table[2] = f;
    }
  }
}
