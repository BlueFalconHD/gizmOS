#include "../../sys/syscall.h"
#include "../../libc-lite/libc.h"
#include "../../lattice/lattice.h"
#include <stdint.h>
#include <stddef.h>
#include "../../include/system_services.h"

static int streq(const char *a, const char *b) {
  size_t i = 0;
  while (a[i] && b[i]) {
    if (a[i] != b[i]) return 0;
    i++;
  }
  return a[i] == '\0' && b[i] == '\0';
}

static void print_line(const char *s) {
  if (s) sys_print_str(s);
  sys_print_str("\n");
}

static void print_paths_from_resp(const lattice_value_t *resp) {
  const lattice_value_t *paths = lat_map_get(resp, "paths");
  if (!paths || paths->type != LAT_T_ARR) {
    sys_print_str("no paths\n");
    return;
  }
  for (size_t i = 0; i < paths->as.arr.count; i++) {
    const lattice_value_t *v = paths->as.arr.items[i];
    if (v && v->type == LAT_T_STR && v->as.str) {
      print_line(v->as.str);
    }
  }
}

int main(int argc, char **argv) {
  lattice_ctx_t *ctx = lattice_init("pathctl");
  if (!ctx) {
    sys_print_str("pathctl: failed to init lattice\n");
    return 1;
  }

  if (argc < 2) {
    sys_print_str("Usage: pathctl <list|add|remove> [path]\n");
    return 1;
  }

  const char *cmd = argv[1];

  if (streq(cmd, "list")) {
    lattice_value_t *req = lat_map_new();
    lattice_value_t *resp = NULL;
    if (lattice_send_message_with_reply_sync(ctx, SERVICE_PATHD, "pathd_list", req, &resp, 1000) == 0 && resp) {
      print_paths_from_resp(resp);
      lat_free(resp);
      lat_free(req);
      return 0;
    }
    lat_free(req);
    sys_print_str("pathctl: list failed\n");
    return 1;
  } else if (streq(cmd, "add")) {
    if (argc < 3) {
      sys_print_str("Usage: pathctl add <path>\n");
      return 1;
    }
    lattice_value_t *req = lat_map_new();
    lat_map_set_str(req, "path", argv[2]);
    lattice_value_t *resp = NULL;
    if (lattice_send_message_with_reply_sync(ctx, SERVICE_PATHD, "pathd_add", req, &resp, 1000) == 0 && resp) {
      int ok = 0;
      (void)lat_map_get_bool(resp, "ok", &ok);
      if (!ok) {
        sys_print_str("pathctl: add failed\n");
      }
      print_paths_from_resp(resp);
      lat_free(resp);
      lat_free(req);
      return ok ? 0 : 1;
    }
    lat_free(req);
    sys_print_str("pathctl: add failed\n");
    return 1;
  } else if (streq(cmd, "remove")) {
    if (argc < 3) {
      sys_print_str("Usage: pathctl remove <path>\n");
      return 1;
    }
    lattice_value_t *req = lat_map_new();
    lat_map_set_str(req, "path", argv[2]);
    lattice_value_t *resp = NULL;
    if (lattice_send_message_with_reply_sync(ctx, SERVICE_PATHD, "pathd_remove", req, &resp, 1000) == 0 && resp) {
      int ok = 0;
      (void)lat_map_get_bool(resp, "ok", &ok);
      if (!ok) {
        sys_print_str("pathctl: remove failed\n");
      }
      print_paths_from_resp(resp);
      lat_free(resp);
      lat_free(req);
      return ok ? 0 : 1;
    }
    lat_free(req);
    sys_print_str("pathctl: remove failed\n");
    return 1;
  } else {
    sys_print_str("Usage: pathctl <list|add|remove> [path]\n");
    return 1;
  }
}
