#include "../../sys/syscall.h"
#include "../../libc-lite/libc.h"
#include "../../lattice/lattice.h"
#include <stdint.h>
#include <stddef.h>

#define MAX_PATH_LEN 160
#define HARD_PATH_COUNT 2

static void __attribute__((noreturn)) spin(void) {
  for (;;) {
  }
}

static const char *kPathList[HARD_PATH_COUNT] = {
    "/vessels",
    "/system/vessels",
};
static lattice_ctx_t *g_ctx = NULL;

// Simple string helpers
static size_t str_len(const char *s) {
  size_t n = 0;
  while (s && s[n]) n++;
  return n;
}

static int str_eq(const char *a, const char *b) {
  size_t i = 0;
  while (a[i] && b[i]) {
    if (a[i] != b[i]) return 0;
    i++;
  }
  return a[i] == '\0' && b[i] == '\0';
}

static void str_copy(char *dst, const char *src, size_t cap) {
  size_t i = 0;
  while (src && src[i] && i + 1 < cap) {
    dst[i] = src[i];
    i++;
  }
  dst[i] = '\0';
}

static int ends_with(const char *s, const char *suffix) {
  size_t len = str_len(s);
  size_t sl = str_len(suffix);
  if (sl > len) return 0;
  for (size_t i = 0; i < sl; i++) {
    if (s[len - sl + i] != suffix[i]) return 0;
  }
  return 1;
}

static void build_with_suffix(char *dst, size_t cap, const char *name, const char *suffix) {
  str_copy(dst, name, cap);
  size_t len = str_len(dst);
  str_copy(dst + len, suffix, cap - len);
}

static void replace_suffix(char *dst, size_t cap, const char *name, const char *old_sfx, const char *new_sfx) {
  size_t len = str_len(name);
  size_t old = str_len(old_sfx);
  size_t base = (len >= old) ? len - old : len;
  if (base >= cap) base = cap - 1;
  for (size_t i = 0; i < base; i++) dst[i] = name[i];
  dst[base] = '\0';
  str_copy(dst + base, new_sfx, cap - base);
}

static int build_path(char *dst, size_t cap, const char *dir, const char *stem) {
  str_copy(dst, dir, cap);
  size_t len = str_len(dst);
  if (len == 0 || dst[len - 1] != '/') {
    if (len + 1 < cap) {
      dst[len++] = '/';
      dst[len] = '\0';
    } else {
      return 0;
    }
  }
  str_copy(dst + len, stem, cap - len);
  return 1;
}

// helper to turn a number into a string (assuming no standard library)
static void num_to_str(char *out, size_t cap, long num) {
  char buf[20];
  int i = 0;
  if (num == 0) {
    out[0] = '0';
    out[1] = '\0';
    return;
  }
  while (num > 0) {
    buf[i++] = '0' + (num % 10);
    num /= 10;
  }
  for (int j = 0; j < i; j++) {
    out[j] = buf[i - j - 1];
  }
  out[i] = '\0';
}

static int try_variant(const char *dir, const char *stem, char *out, size_t cap) {
  if (!build_path(out, cap, dir, stem)) return 0;

  long h = sys_obj_id_at(out);

  if (h >= 0) {
    return 1;
  }
  return 0;
}

static void attach_paths_array(lattice_value_t *dest) {
  lattice_value_t *arr = lat_array_new();
  if (!arr) return;
  for (size_t i = 0; i < HARD_PATH_COUNT; i++) {
    lat_array_push(arr, lat_str(kPathList[i]));
  }
  lat_map_set(dest, "paths", arr);
}

static void handle_locate(lattice_ctx_t *ctx, const lattice_value_t *req, lattice_value_t **resp, void *user) {
  (void)ctx; (void)user;
  const char *name = NULL;
  if (!lat_map_get_str(req, "name", &name) || !name) {
    *resp = lat_map_new();
    lat_map_set_bool(*resp, "ok", 0);
    return;
  }

  lattice_value_t *out = lat_map_new();
  if (!out) return;
  int found = 0;
  for (size_t i = 0; i < HARD_PATH_COUNT; i++) {
    char full[200];
    if (try_variant(kPathList[i], name, full, sizeof(full))) {
      lat_map_set_bool(out, "ok", 1);
      lat_map_set_str(out, "path", full);
      found = 1;
      break;
    }
  }
  if (!found) {
    lat_map_set_bool(out, "ok", 0);
  }
  *resp = out;
}

static void handle_add(lattice_ctx_t *ctx, const lattice_value_t *req, lattice_value_t **resp, void *user) {
  (void)ctx; (void)req; (void)user;
  lattice_value_t *out = lat_map_new();
  lat_map_set_bool(out, "ok", 0);
  attach_paths_array(out);
  *resp = out;
}

static void handle_remove(lattice_ctx_t *ctx, const lattice_value_t *req, lattice_value_t **resp, void *user) {
  (void)ctx; (void)req; (void)user;
  lattice_value_t *out = lat_map_new();
  lat_map_set_bool(out, "ok", 0);
  attach_paths_array(out);
  *resp = out;
}

static void handle_list(lattice_ctx_t *ctx, const lattice_value_t *req, lattice_value_t **resp, void *user) {
  (void)ctx; (void)req; (void)user;
  lattice_value_t *out = lat_map_new();
  attach_paths_array(out);
  *resp = out;
}

int main(void) {
  g_ctx = lattice_init("pathd");
  if (!g_ctx) {
    sys_print_str("pathd: failed to init lattice\n");
    sys_exit(-1);
  }
  lattice_register(g_ctx, "pathd_locate", handle_locate, NULL);
  lattice_register(g_ctx, "pathd_add", handle_add, NULL);
  lattice_register(g_ctx, "pathd_remove", handle_remove, NULL);
  lattice_register(g_ctx, "pathd_list", handle_list, NULL);

  // Keep alive
  while (1) {
    spin();
  }
  return 0;
}
