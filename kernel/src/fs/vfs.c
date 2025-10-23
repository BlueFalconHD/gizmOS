#include "vfs.h"
#include <device/shared.h>
#include <lib/kalloc.h>
#include <lib/memory.h>
#include <lib/print.h>
#include <lib/str.h>

static vfs_mount_t *g_root_mount = NULL;

void vfs_init(void) {
  g_root_mount = NULL;
}

RESULT_TYPE(void) vfs_set_root(vfs_mount_t *mnt) {
  g_root_mount = mnt;
  return RESULT_SUCCESS(0);
}

result_t vfs_mount_root(disk_t *disk, const vfs_fs_type_t *type) {
  if (!disk || !type || !type->mount)
    return RESULT_FAILURE(RESULT_INVALID);
  vfs_mount_t *mnt = NULL;
  result_t rm = type->mount(disk, &mnt);
  if (!result_is_ok(rm))
    return rm;
  g_root_mount = mnt;
  return RESULT_SUCCESS(0);
}

static inline g_bool is_slash(char c) { return c == '/'; }

static const char *skip_slashes(const char *p) {
  while (*p && is_slash(*p)) p++;
  return p;
}

/* Extract next path component into name buffer. Returns pointer to next char after component. */
static const char *next_component(const char *p, char *name_buf, size_t *len) {
  p = skip_slashes(p);
  size_t i = 0;
  while (p[i] && !is_slash(p[i])) {
    if (i < 64) name_buf[i] = p[i];
    i++;
  }
  size_t n = (i < 64) ? i : 64;
  if (n < 64) name_buf[n] = '\0'; else name_buf[63] = '\0';
  if (len) *len = n;
  return p + i;
}

static g_bool name_is_dot(const char *s) { return s[0] == '.' && s[1] == '\0'; }
static g_bool name_is_dotdot(const char *s) { return s[0] == '.' && s[1] == '.' && s[2] == '\0'; }

result_t vfs_lookup(const char *path, vfs_node_t **out) {
  if (!g_root_mount || !g_root_mount->ops || !g_root_mount->root)
    return RESULT_FAILURE(RESULT_ERROR);
  if (!path || !out) return RESULT_FAILURE(RESULT_INVALID);

  vfs_mount_t *mnt = g_root_mount;
  vfs_node_t *cur = mnt->root;

  const char *p = path;
  if (is_slash(*p)) {
    p = skip_slashes(p);
  }

  /* Empty or '/' resolves to root */
  if (!*p) {
    *out = cur;
    return RESULT_SUCCESS(0);
  }

  char name[65];
  size_t nlen = 0;
  while (*p) {
    p = next_component(p, name, &nlen);
    if (nlen == 0) break;
    if (name_is_dot(name)) {
      // stay at cur
    } else if (name_is_dotdot(name)) {
      // no parent tracking yet; stay at root
      cur = mnt->root;
    } else {
      vfs_node_t *next = NULL;
      result_t rlk = mnt->ops->lookup(mnt, cur, name, &next);
      if (!result_is_ok(rlk)) return rlk;
      cur = next;
    }
    p = skip_slashes(p);
  }

  *out = cur;
  return RESULT_SUCCESS(0);
}

result_t vfs_getattr(const char *path, vfs_stat_t *st) {
  if (!st) return RESULT_FAILURE(RESULT_INVALID);
  vfs_node_t *node = NULL;
  result_t rl = vfs_lookup(path, &node);
  if (!result_is_ok(rl)) return rl;
  return g_root_mount->ops->getattr(g_root_mount, node, st);
}

result_t vfs_read_entire(const char *path, void *buf, size_t cap, size_t *n) {
  if (!buf) return RESULT_FAILURE(RESULT_INVALID);
  vfs_node_t *node = NULL;
  result_t rl = vfs_lookup(path, &node);
  if (!result_is_ok(rl)) return rl;
  vfs_stat_t st;
  result_t rs = g_root_mount->ops->getattr(g_root_mount, node, &st);
  if (!result_is_ok(rs)) return rs;
  size_t to_read = (st.size <= cap) ? (size_t)st.size : cap;
  size_t total = 0;
  while (total < to_read) {
    size_t chunk = to_read - total;
    size_t outn = 0;
    result_t rr = g_root_mount->ops->read(g_root_mount, node, total,
                                          (uint8_t *)buf + total, chunk, &outn);
    if (!result_is_ok(rr)) return rr;
    if (outn == 0) break;
    total += outn;
  }
  if (n) *n = total;
  return RESULT_SUCCESS(0);
}

vfs_mount_t *vfs_root_mount(void) { return g_root_mount; }
vfs_node_t *vfs_root_node(void) { return g_root_mount ? g_root_mount->root : NULL; }


