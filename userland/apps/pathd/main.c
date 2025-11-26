#include "../../sys/syscall.h"
#include "../../libc-lite/libc.h"
#include "../../lattice/lattice.h"
#include <stdint.h>
#include <stddef.h>

#define MAX_PATH_LEN 160
#define HARD_PATH_COUNT 2
#define PATHD_STORE_DIR  "/system/vessels/pathd.vessel"
#define PATHD_STORE_FILE "paths.bin"

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
  // 1) Try as provided
  if (!build_path(out, cap, dir, stem)) return 0;
  long h = sys_obj_id_at(out);
  if (h >= 0) return 1;

  // 2) If no extension provided, try with \".vessel\"
  if (!ends_with(stem, ".vessel")) {
    char name2[164];
    build_with_suffix(name2, sizeof(name2), stem, ".vessel");
    if (build_path(out, cap, dir, name2)) {
      h = sys_obj_id_at(out);
      if (h >= 0) return 1;
    }
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

static char **g_paths = NULL;
static size_t g_paths_count = 0;
static size_t g_paths_cap = 0;

static void path_list_clear(void) {
  if (!g_paths) return;
  for (size_t i = 0; i < g_paths_count; i++) {
    if (g_paths[i]) free(g_paths[i]);
  }
  g_paths_count = 0;
}

static void path_list_ensure(size_t need) {
  if (g_paths_cap >= need) return;
  size_t ncap = g_paths_cap ? g_paths_cap * 2 : 4;
  while (ncap < need) ncap *= 2;
  char **n = (char **)realloc(g_paths, ncap * sizeof(char *));
  if (!n) return;
  g_paths = n;
  g_paths_cap = ncap;
}

static int path_list_contains(const char *p) {
  for (size_t i = 0; i < g_paths_count; i++) {
    if (g_paths[i] && str_eq(g_paths[i], p)) return 1;
  }
  return 0;
}

static int path_list_add(const char *p) {
  if (!p || !p[0]) return 0;
  if (str_len(p) >= MAX_PATH_LEN) return 0;
  if (path_list_contains(p)) return 1;
  path_list_ensure(g_paths_count + 1);
  if (g_paths_cap < g_paths_count + 1) return 0;
  size_t n = str_len(p);
  char *cp = (char *)malloc(n + 1);
  if (!cp) return 0;
  for (size_t i = 0; i < n; i++) cp[i] = p[i];
  cp[n] = '\0';
  g_paths[g_paths_count++] = cp;
  return 1;
}

static int path_list_remove(const char *p) {
  if (!p) return 0;
  for (size_t i = 0; i < g_paths_count; i++) {
    if (g_paths[i] && str_eq(g_paths[i], p)) {
      free(g_paths[i]);
      for (size_t j = i + 1; j < g_paths_count; j++) g_paths[j - 1] = g_paths[j];
      g_paths_count--;
      return 1;
    }
  }
  return 0;
}

static void path_list_init_default(void) {
  path_list_clear();
  for (size_t i = 0; i < HARD_PATH_COUNT; i++) {
    (void)path_list_add(kPathList[i]);
  }
}

static void attach_current_paths_array(lattice_value_t *dest) {
  lattice_value_t *arr = lat_array_new();
  if (!arr) return;
  for (size_t i = 0; i < g_paths_count; i++) {
    lat_array_push(arr, lat_str(g_paths[i] ? g_paths[i] : ""));
  }
  lat_map_set(dest, "paths", arr);
}

static int path_store_open_or_create(long *out_handle) {
  char full[256];
  if (!build_path(full, sizeof(full), PATHD_STORE_DIR, PATHD_STORE_FILE)) return 0;
  long fh = sys_objh_open_at(full, 0);
  if (fh >= 0) { *out_handle = fh; return 1; }
  long dh = sys_objh_open_at(PATHD_STORE_DIR, 0);
  if (dh < 0) return 0;
  // Pass kind=0; kernel ignores it and sets capabilities on first use
  long nh = sys_objh_create(dh, PATHD_STORE_FILE, 0, 0);
  sys_objh_close(dh);
  if (nh < 0) return 0;
  *out_handle = nh;
  return 1;
}

static int path_store_open_read(long *out_handle) {
  char full[256];
  if (!build_path(full, sizeof(full), PATHD_STORE_DIR, PATHD_STORE_FILE)) return 0;
  long fh = sys_objh_open_at(full, 0);
  if (fh < 0) return 0;
  *out_handle = fh;
  return 1;
}

static int path_store_save(void) {
  lattice_value_t *arr = lat_array_new();
  if (!arr) return 0;
  for (size_t i = 0; i < g_paths_count; i++) {
    lat_array_push(arr, lat_str(g_paths[i] ? g_paths[i] : ""));
  }
  void *enc = NULL; unsigned long enc_len = 0;
  if (lat_encode(arr, &enc, &enc_len) != 0) { lat_free(arr); return 0; }
  lat_free(arr);
  unsigned char *blob = (unsigned char *)malloc(enc_len + 4);
  if (!blob) { free(enc); return 0; }
  uint32_t n = (uint32_t)enc_len;
  blob[0] = (unsigned char)(n & 0xFF);
  blob[1] = (unsigned char)((n >> 8) & 0xFF);
  blob[2] = (unsigned char)((n >> 16) & 0xFF);
  blob[3] = (unsigned char)((n >> 24) & 0xFF);
  for (uint32_t i = 0; i < n; i++) blob[4 + i] = ((unsigned char *)enc)[i];
  free(enc);
  long h = -1;
  if (!path_store_open_or_create(&h)) { free(blob); return 0; }
  long wrote = sys_objh_write(h, blob, 0, (long)(enc_len + 4));
  sys_objh_close(h);
  free(blob);
  return wrote == (long)(enc_len + 4);
}

static int path_store_load(void) {
  long h = -1;
  if (!path_store_open_read(&h)) return 0;
  unsigned char hdr[4];
  long r = sys_objh_read(h, hdr, 0, 4);
  if (r != 4) { sys_objh_close(h); return 0; }
  uint32_t n = (uint32_t)(hdr[0] | (hdr[1] << 8) | (hdr[2] << 16) | (hdr[3] << 24));
  if (n == 0 || n > 4096u) { sys_objh_close(h); return 0; }
  unsigned char *buf = (unsigned char *)malloc(n);
  if (!buf) { sys_objh_close(h); return 0; }
  unsigned long off = 0;
  while (off < n) {
    long chunk = sys_objh_read(h, buf + off, 4 + off, (long)(n - off));
    if (chunk <= 0) { free(buf); sys_objh_close(h); return 0; }
    off += (unsigned long)chunk;
  }
  sys_objh_close(h);
  lattice_value_t *root = NULL;
  if (lat_decode(buf, n, &root) != 0) { free(buf); return 0; }
  free(buf);
  if (!root || root->type != LAT_T_ARR) { if (root) lat_free(root); return 0; }
  path_list_clear();
  for (size_t i = 0; i < root->as.arr.count; i++) {
    lattice_value_t *v = root->as.arr.items[i];
    if (v && v->type == LAT_T_STR && v->as.str) {
      (void)path_list_add(v->as.str);
    }
  }
  lat_free(root);
  return 1;
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
  for (size_t i = 0; i < g_paths_count; i++) {
    char full[200];
    if (try_variant(g_paths[i], name, full, sizeof(full))) {
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
  (void)ctx; (void)user;
  lattice_value_t *out = lat_map_new();
  int ok = 0;
  const char *p = NULL;
  if (req && lat_map_get_str(req, "path", &p) && p && p[0]) {
    if (path_list_add(p)) {
      ok = path_store_save();
      if (!ok) {
        /* keep in-memory add even if save fails */
        ok = 1;
      }
    }
  }
  lat_map_set_bool(out, "ok", ok ? 1 : 0);
  attach_current_paths_array(out);
  *resp = out;
}

static void handle_remove(lattice_ctx_t *ctx, const lattice_value_t *req, lattice_value_t **resp, void *user) {
  (void)ctx; (void)user;
  lattice_value_t *out = lat_map_new();
  int ok = 0;
  const char *p = NULL;
  if (req && lat_map_get_str(req, "path", &p) && p && p[0]) {
    if (path_list_remove(p)) {
      ok = path_store_save();
      if (!ok) {
        ok = 1;
      }
    }
  }
  lat_map_set_bool(out, "ok", ok ? 1 : 0);
  attach_current_paths_array(out);
  *resp = out;
}

static void handle_list(lattice_ctx_t *ctx, const lattice_value_t *req, lattice_value_t **resp, void *user) {
  (void)ctx; (void)req; (void)user;
  lattice_value_t *out = lat_map_new();
  attach_current_paths_array(out);
  *resp = out;
}

int main(void) {
  g_ctx = lattice_init("pathd");
  if (!g_ctx) {
    sys_print_str("pathd: failed to init lattice\n");
    sys_exit(-1);
  }
  if (!path_store_load()) {
    path_list_init_default();
    (void)path_store_save();
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
