#include "lattice.h"

// ------------- Utility -------------
static int streq(const char *a, const char *b) {
  size_t i = 0;
  while (a[i] && b[i]) {
    if (a[i] != b[i]) return 0;
    i++;
  }
  return a[i] == '\0' && b[i] == '\0';
}

static char *str_dup(const char *s) {
  size_t n = strlen(s);
  char *out = (char *)malloc(n + 1);
  if (!out) return NULL;
  for (size_t i = 0; i < n; i++) out[i] = s[i];
  out[n] = '\0';
  return out;
}

// ------------- Value builders -------------
lattice_value_t *lat_null() {
  lattice_value_t *v = (lattice_value_t *)malloc(sizeof(*v));
  if (!v) return NULL;
  v->type = LAT_T_NULL;
  return v;
}

lattice_value_t *lat_bool(int b) {
  lattice_value_t *v = (lattice_value_t *)malloc(sizeof(*v));
  if (!v) return NULL;
  v->type = LAT_T_BOOL;
  v->as.b = b ? 1 : 0;
  return v;
}

lattice_value_t *lat_int(int64_t x) {
  lattice_value_t *v = (lattice_value_t *)malloc(sizeof(*v));
  if (!v) return NULL;
  v->type = LAT_T_INT;
  v->as.i64 = x;
  return v;
}

lattice_value_t *lat_str(const char *s) {
  lattice_value_t *v = (lattice_value_t *)malloc(sizeof(*v));
  if (!v) return NULL;
  v->type = LAT_T_STR;
  v->as.str = str_dup(s ? s : "");
  if (!v->as.str) { free(v); return NULL; }
  return v;
}

lattice_value_t *lat_map_new() {
  lattice_value_t *v = (lattice_value_t *)malloc(sizeof(*v));
  if (!v) return NULL;
  v->type = LAT_T_MAP;
  v->as.map.entries = NULL;
  v->as.map.count = 0;
  v->as.map.capacity = 0;
  return v;
}

lattice_value_t *lat_array_new() {
  lattice_value_t *v = (lattice_value_t *)malloc(sizeof(*v));
  if (!v) return NULL;
  v->type = LAT_T_ARR;
  v->as.arr.items = NULL;
  v->as.arr.count = 0;
  v->as.arr.capacity = 0;
  return v;
}

static void lat_free_inner(lattice_value_t *v) {
  if (!v) return;
  switch (v->type) {
  case LAT_T_NULL:
  case LAT_T_INT:
  case LAT_T_BOOL:
    break;
  case LAT_T_STR:
    if (v->as.str) free(v->as.str);
    v->as.str = NULL;
    break;
  case LAT_T_MAP:
    if (v->as.map.entries) {
      for (size_t i = 0; i < v->as.map.count; i++) {
        if (v->as.map.entries[i].key) free(v->as.map.entries[i].key);
        if (v->as.map.entries[i].value) lat_free(v->as.map.entries[i].value);
      }
      free(v->as.map.entries);
      v->as.map.entries = NULL;
      v->as.map.count = v->as.map.capacity = 0;
    }
    break;
  case LAT_T_ARR:
    if (v->as.arr.items) {
      for (size_t i = 0; i < v->as.arr.count; i++) {
        if (v->as.arr.items[i]) lat_free(v->as.arr.items[i]);
      }
      free(v->as.arr.items);
      v->as.arr.items = NULL;
      v->as.arr.count = v->as.arr.capacity = 0;
    }
    break;
  }
}

void lat_free(lattice_value_t *v) {
  if (!v) return;
  lat_free_inner(v);
  free(v);
}

static void map_ensure(lattice_value_t *map, size_t need) {
  if (map->as.map.capacity >= need) return;
  size_t newcap = map->as.map.capacity ? map->as.map.capacity * 2 : 4;
  while (newcap < need) newcap *= 2;
  lattice_map_entry_t *n = (lattice_map_entry_t *)realloc(map->as.map.entries, newcap * sizeof(lattice_map_entry_t));
  if (!n) return; // best-effort; caller should check count before use
  map->as.map.entries = n;
  map->as.map.capacity = newcap;
}

void lat_map_set(lattice_value_t *map, const char *key, lattice_value_t *v) {
  if (!map || map->type != LAT_T_MAP || !key) return;
  // Overwrite if exists
  for (size_t i = 0; i < map->as.map.count; i++) {
    if (streq(map->as.map.entries[i].key, key)) {
      if (map->as.map.entries[i].value) lat_free(map->as.map.entries[i].value);
      map->as.map.entries[i].value = v;
      return;
    }
  }
  map_ensure(map, map->as.map.count + 1);
  size_t i = map->as.map.count++;
  map->as.map.entries[i].key = str_dup(key);
  map->as.map.entries[i].value = v;
}

void lat_map_set_str(lattice_value_t *map, const char *key, const char *s) {
  lat_map_set(map, key, lat_str(s));
}
void lat_map_set_int(lattice_value_t *map, const char *key, int64_t v) {
  lat_map_set(map, key, lat_int(v));
}
void lat_map_set_bool(lattice_value_t *map, const char *key, int b) {
  lat_map_set(map, key, lat_bool(b));
}

lattice_value_t *lat_map_get(const lattice_value_t *map, const char *key) {
  if (!map || map->type != LAT_T_MAP || !key) return NULL;
  for (size_t i = 0; i < map->as.map.count; i++) {
    if (streq(map->as.map.entries[i].key, key)) {
      return map->as.map.entries[i].value;
    }
  }
  return NULL;
}

int lat_map_get_str(const lattice_value_t *map, const char *key, const char **out_str) {
  lattice_value_t *v = lat_map_get(map, key);
  if (!v || v->type != LAT_T_STR) return 0;
  if (out_str) *out_str = v->as.str;
  return 1;
}
int lat_map_get_int(const lattice_value_t *map, const char *key, int64_t *out_v) {
  lattice_value_t *v = lat_map_get(map, key);
  if (!v || v->type != LAT_T_INT) return 0;
  if (out_v) *out_v = v->as.i64;
  return 1;
}
int lat_map_get_bool(const lattice_value_t *map, const char *key, int *out_b) {
  lattice_value_t *v = lat_map_get(map, key);
  if (!v || v->type != LAT_T_BOOL) return 0;
  if (out_b) *out_b = v->as.b;
  return 1;
}

static void arr_ensure(lattice_value_t *arr, size_t need) {
  if (arr->as.arr.capacity >= need) return;
  size_t newcap = arr->as.arr.capacity ? arr->as.arr.capacity * 2 : 4;
  while (newcap < need) newcap *= 2;
  lattice_value_t **n = (lattice_value_t **)realloc(arr->as.arr.items, newcap * sizeof(lattice_value_t *));
  if (!n) return;
  arr->as.arr.items = n;
  arr->as.arr.capacity = newcap;
}
void lat_array_push(lattice_value_t *arr, lattice_value_t *v) {
  if (!arr || arr->type != LAT_T_ARR) return;
  arr_ensure(arr, arr->as.arr.count + 1);
  arr->as.arr.items[arr->as.arr.count++] = v;
}

// ------------- Encoding (TLV) -------------
// Wire types
enum { W_NULL=0, W_BOOL=1, W_INT=2, W_STR=3, W_MAP=4, W_ARR=5 };

typedef struct {
  unsigned char *data;
  unsigned long len;
  unsigned long cap;
} bw_t;

static int bw_reserve(bw_t *bw, unsigned long add) {
  if (bw->len + add <= bw->cap) return 1;
  unsigned long ncap = bw->cap ? bw->cap * 2 : 128;
  while (ncap < bw->len + add) ncap *= 2;
  void *p = realloc(bw->data, ncap);
  if (!p) return 0;
  bw->data = (unsigned char *)p;
  bw->cap = ncap;
  return 1;
}
static int bw_put(bw_t *bw, const void *src, unsigned long n) {
  if (!bw_reserve(bw, n)) return 0;
  memcpy(bw->data + bw->len, src, n);
  bw->len += n;
  return 1;
}
static int bw_put_u8(bw_t *bw, unsigned char v) { return bw_put(bw, &v, 1); }
static int bw_put_u32(bw_t *bw, uint32_t v) { return bw_put(bw, &v, 4); }
static int bw_put_u64(bw_t *bw, uint64_t v) { return bw_put(bw, &v, 8); }

static int enc_value(bw_t *bw, const lattice_value_t *v);

static int enc_map(bw_t *bw, const lattice_map_t *m) {
  if (!bw_put_u8(bw, W_MAP)) return 0;
  if (!bw_put_u32(bw, (uint32_t)m->count)) return 0;
  for (size_t i = 0; i < m->count; i++) {
    // key
    const char *k = m->entries[i].key ? m->entries[i].key : "";
    uint32_t n = (uint32_t)strlen(k);
    if (!bw_put_u8(bw, W_STR)) return 0;
    if (!bw_put_u32(bw, n)) return 0;
    if (!bw_put(bw, k, n)) return 0;
    // value
    if (!enc_value(bw, m->entries[i].value)) return 0;
  }
  return 1;
}

static int enc_array(bw_t *bw, const lattice_array_t *a) {
  if (!bw_put_u8(bw, W_ARR)) return 0;
  if (!bw_put_u32(bw, (uint32_t)a->count)) return 0;
  for (size_t i = 0; i < a->count; i++) {
    if (!enc_value(bw, a->items[i])) return 0;
  }
  return 1;
}

static int enc_value(bw_t *bw, const lattice_value_t *v) {
  switch (v->type) {
  case LAT_T_NULL: return bw_put_u8(bw, W_NULL);
  case LAT_T_BOOL:
    if (!bw_put_u8(bw, W_BOOL)) return 0;
    return bw_put_u8(bw, v->as.b ? 1 : 0);
  case LAT_T_INT:
    if (!bw_put_u8(bw, W_INT)) return 0;
    return bw_put_u64(bw, (uint64_t)v->as.i64);
  case LAT_T_STR: {
    if (!bw_put_u8(bw, W_STR)) return 0;
    uint32_t n = (uint32_t)strlen(v->as.str ? v->as.str : "");
    if (!bw_put_u32(bw, n)) return 0;
    return n ? bw_put(bw, v->as.str, n) : 1;
  }
  case LAT_T_MAP:
    return enc_map(bw, &v->as.map);
  case LAT_T_ARR:
    return enc_array(bw, &v->as.arr);
  }

  sys_print_str("enc_value: unknown type\n");
  return 0;
}

int lat_encode(const lattice_value_t *v, void **out_buf, unsigned long *out_len) {
  if (!v || !out_buf || !out_len) return -1;
  bw_t bw = {0};
  if (!enc_value(&bw, v)) {
    sys_print_str("lat_encode: failed to encode value\n");
    if (bw.data) free(bw.data);
    return -1;
  }
  *out_buf = bw.data;
  *out_len = bw.len;
  return 0;
}

typedef struct {
  const unsigned char *p;
  unsigned long n;
} br_t;
static int br_need(br_t *br, unsigned long k) { return br->n >= k; }
static int br_u8(br_t *br, unsigned char *out) {
  if (!br_need(br,1)) return 0;
  *out = br->p[0];
  br->p++; br->n--;
  return 1;
}
static int br_u32(br_t *br, uint32_t *out) {
  if (!br_need(br,4)) return 0;
  const unsigned char *s = br->p;
  *out = (uint32_t)(s[0] | (s[1]<<8) | (s[2]<<16) | (s[3]<<24));
  br->p += 4; br->n -= 4;
  return 1;
}
static int br_u64(br_t *br, uint64_t *out) {
  if (!br_need(br,8)) return 0;
  const unsigned char *s = br->p;
  *out = ((uint64_t)s[0]) |
         ((uint64_t)s[1] << 8) |
         ((uint64_t)s[2] << 16) |
         ((uint64_t)s[3] << 24) |
         ((uint64_t)s[4] << 32) |
         ((uint64_t)s[5] << 40) |
         ((uint64_t)s[6] << 48) |
         ((uint64_t)s[7] << 56);
  br->p += 8; br->n -= 8;
  return 1;
}
static int dec_value(br_t *br, lattice_value_t **out);

static int dec_map(br_t *br, lattice_value_t **out) {
  lattice_value_t *m = lat_map_new();
  if (!m) return 0;
  uint32_t cnt = 0;
  if (!br_u32(br, &cnt)) { lat_free(m); return 0; }
  for (uint32_t i = 0; i < cnt; i++) {
    // key (must be STR)
    unsigned char t = 0;
    if (!br_u8(br, &t) || t != W_STR) { lat_free(m); return 0; }
    uint32_t n = 0;
    if (!br_u32(br, &n)) { lat_free(m); return 0; }
    char *k = (char *)malloc(n + 1);
    if (!k) { lat_free(m); return 0; }
    for (uint32_t j = 0; j < n; j++) {
      if (!br_need(br,1)) { free(k); lat_free(m); return 0; }
      k[j] = (char)br->p[0]; br->p++; br->n--;
    }
    k[n] = '\0';
    lattice_value_t *val = NULL;
    if (!dec_value(br, &val)) { free(k); lat_free(m); return 0; }
    // set
    lat_map_set(m, k, val);
    free(k); // lat_map_set duplicated key
  }
  *out = m;
  return 1;
}

static int dec_array(br_t *br, lattice_value_t **out) {
  lattice_value_t *a = lat_array_new();
  if (!a) return 0;
  uint32_t cnt = 0;
  if (!br_u32(br, &cnt)) { lat_free(a); return 0; }
  for (uint32_t i = 0; i < cnt; i++) {
    lattice_value_t *v = NULL;
    if (!dec_value(br, &v)) { lat_free(a); return 0; }
    lat_array_push(a, v);
  }
  *out = a;
  return 1;
}

static int dec_value(br_t *br, lattice_value_t **out) {
  unsigned char t = 0;
  if (!br_u8(br, &t)) return 0;
  switch (t) {
  case W_NULL:
    *out = lat_null(); return *out != NULL;
  case W_BOOL: {
    unsigned char b = 0; if (!br_u8(br, &b)) return 0;
    *out = lat_bool(b ? 1 : 0); return *out != NULL;
  }
  case W_INT: {
    uint64_t v = 0; if (!br_u64(br, &v)) return 0;
    *out = lat_int((int64_t)v); return *out != NULL;
  }
  case W_STR: {
    uint32_t n = 0; if (!br_u32(br, &n)) return 0;
    char *s = (char *)malloc(n + 1);
    if (!s) return 0;
    for (uint32_t i = 0; i < n; i++) {
      if (!br_need(br,1)) { free(s); return 0; }
      s[i] = (char)br->p[0]; br->p++; br->n--;
    }
    s[n] = '\0';
    lattice_value_t *v = lat_str(s);
    free(s);
    if (!v) return 0;
    *out = v; return 1;
  }
  case W_MAP:
    return dec_map(br, out);
  case W_ARR:
    return dec_array(br, out);
  default:
    return 0;
  }
}

int lat_decode(const void *buf, unsigned long len, lattice_value_t **out_v) {
  if (!buf || !out_v) return -1;
  br_t br = { .p = (const unsigned char *)buf, .n = len };
  lattice_value_t *v = NULL;
  if (!dec_value(&br, &v)) return -1;
  *out_v = v;
  return 0;
}

// ------------- Lattice IPC -------------
typedef struct {
  char name[32];
  lattice_method_cb cb;
  void *user;
} method_entry_t;

typedef struct {
  uint64_t id;
  long dest_pid;
  lattice_reply_cb cb;
  void *user;
  lattice_value_t *resp; // for sync
  int done;
} pending_entry_t;

struct lattice_ctx {
  char service[17];
  method_entry_t methods[16];
  int methods_count;
  pending_entry_t pending[32];
  int pending_count;
  uint64_t next_id;
  void *rxbuf;
  unsigned long rxcap;
};

typedef struct __attribute__((packed)) {
  uint64_t sender_token;
  uint32_t message_size;
  uint32_t reserved;
} spine_wire_msg_t;

static void lattice_dispatch_request(lattice_ctx_t *ctx,
                                     const spine_wire_msg_t *hdr,
                                     const lattice_value_t *root) {
  const char *method = NULL;
  if (!lat_map_get_str(root, "method", &method) || !method) {
    sys_print_str("lattice_dispatch_request: missing method\n");
    return;
  }
  int reply_expected = 0;
  (void)lat_map_get_bool(root, "reply_expected", &reply_expected);
  int64_t req_id = 0;
  (void)lat_map_get_int(root, "id", &req_id);

  // args (optional)
  lattice_value_t *args = lat_map_get(root, "args");

  // Find method
  lattice_method_cb cb = NULL;
  void *user = NULL;
  for (int i = 0; i < ctx->methods_count; i++) {
    if (streq(ctx->methods[i].name, method)) {
      cb = ctx->methods[i].cb;
      user = ctx->methods[i].user;
      break;
    }
  }
  if (!cb) {
      sys_print_str("lattice_dispatch_request: unknown method '");
      sys_print_str(method);
      sys_print_str("'\n");
      return;
  };

  lattice_value_t *resp = NULL;
  cb(ctx, args, &resp, user);

  if (reply_expected) {
    // Reply back to sender pid
    sys_spine_seal_t seal;
    if (sys_spine_get_seal(hdr->sender_token, &seal, (long)sizeof(seal)) == 0) {
      lattice_value_t *out = lat_map_new();
      lat_map_set_bool(out, "resp", 1);
      lat_map_set_int(out, "id", req_id);
      if (resp) {
        lat_map_set(out, "result", resp);
      } else {
        lat_map_set_null:
        lat_map_set(out, "result", lat_null());
      }
      void *buf = NULL; unsigned long n = 0;
      if (lat_encode(out, &buf, &n) == 0) {
        sys_spine_msg_send(seal.pid, buf, (long)n, 0);
        free(buf);
      }
      lat_free(out);
    }
    if (resp) lat_free(resp);
  } else {
    if (resp) lat_free(resp);
  }
}

static void lattice_dispatch_response(lattice_ctx_t *ctx, const lattice_value_t *root) {
  int64_t id = 0;
  if (!lat_map_get_int(root, "id", &id)) return;
  lattice_value_t *result = lat_map_get(root, "result");
  // locate pending
  for (int i = 0; i < ctx->pending_count; i++) {
    if (ctx->pending[i].id == (uint64_t)id && !ctx->pending[i].done) {
      lattice_value_t *copy = NULL;
      // We can shallow-copy since we won't reuse root; but safer: deep copy not implemented; instead, pass pointer and don't free here.
      // For async: invoke callback immediately
      if (ctx->pending[i].cb) {
        ctx->pending[i].cb(ctx, result, ctx->pending[i].user);
        ctx->pending[i].done = 1;
      } else {
        // sync waiter: store result (take ownership by duplicating encode/decode)
        void *buf = NULL; unsigned long n = 0;
        if (lat_encode(result, &buf, &n) == 0) {
          lattice_value_t *dup = NULL;
          if (lat_decode(buf, n, &dup) == 0) {
            ctx->pending[i].resp = dup;
            ctx->pending[i].done = 1;
          }
          free(buf);
        }
      }
      break;
    }
  }
}

lattice_ctx_t *lattice_init(const char *service_name) {
  lattice_ctx_t *ctx = (lattice_ctx_t *)calloc(1, sizeof(*ctx));
  if (!ctx) return NULL;
  // Copy service name (<=16 per Spine)
  size_t n = 0; while (service_name && service_name[n] && n < 16) { ctx->service[n] = service_name[n]; n++; }
  ctx->service[n] = '\0';
  if (service_name && service_name[0]) {
    (void)sys_spine_service_advertise(ctx->service, 0);
  }
  ctx->rxcap = 64UL * 1024UL;
  ctx->rxbuf = calloc(1, ctx->rxcap);
  if (!ctx->rxbuf) {
    free(ctx);
    return NULL;
  }
  ctx->next_id = 1;
  return ctx;
}

int lattice_poll(lattice_ctx_t *ctx, unsigned long timeout_ticks) {
  if (!ctx || !ctx->rxbuf || ctx->rxcap == 0) return -1;

  long got = 0;
  uint64_t sender_token = 0;
  long rc = sys_spine_msg_recv(ctx->rxbuf, (long)ctx->rxcap, &got, &sender_token, timeout_ticks);
  if (rc == -2) return 0;   // timeout
  if (rc != 0) return -1;
  if (got <= 0) return -1;

  spine_wire_msg_t hdr = {
    .sender_token = sender_token,
    .message_size = (uint32_t)got,
    .reserved = 0,
  };

  unsigned long mlen = (unsigned long)got;
  void *msg = ctx->rxbuf;

  lattice_value_t *root = NULL;
  if (lat_decode(msg, mlen, &root) != 0) {
    return -1;
  }
  int is_resp = 0;
  if (lat_map_get_bool(root, "resp", &is_resp) && is_resp) {
    lattice_dispatch_response(ctx, root);
  } else {
    lattice_dispatch_request(ctx, &hdr, root);
  }
  lat_free(root);
  return 1;
}

int lattice_register(lattice_ctx_t *ctx, const char *method, lattice_method_cb cb, void *user) {
  if (!ctx || !method || !cb) return -1;
  if (ctx->methods_count >= (int)(sizeof(ctx->methods)/sizeof(ctx->methods[0]))) return -1;
  int i = ctx->methods_count++;
  // Copy method name (truncate to 31)
  size_t n = 0; while (method[n] && n < sizeof(ctx->methods[i].name)-1) { ctx->methods[i].name[n] = method[n]; n++; }
  ctx->methods[i].name[n] = '\0';
  ctx->methods[i].cb = cb;
  ctx->methods[i].user = user;
  return 0;
}

static int lattice_send_enveloped(long dest_pid,
                                  const char *method,
                                  const lattice_value_t *args,
                                  int with_reply, uint64_t id) {
  lattice_value_t *root = lat_map_new();
  if (!root) return -1;
  lat_map_set_str(root, "method", method ? method : "");
  if (args) lat_map_set(root, "args", (lattice_value_t *)args); // NOTE: cast away const is unsafe; duplicate instead
  // duplicate args via encode/decode to avoid ownership issues
  if (args) {
    void *buf = NULL; unsigned long n = 0;
    if (lat_encode(args, &buf, &n) == 0) {
      lattice_value_t *dup = NULL;
      if (lat_decode(buf, n, &dup) == 0) {
        lat_map_set(root, "args", dup);
      }
      free(buf);
    }
  }
  if (with_reply) {
    lat_map_set_bool(root, "reply_expected", 1);
    lat_map_set_int(root, "id", (int64_t)id);
  }
  void *buf = NULL; unsigned long n = 0;
  int rc = -1;
  if (lat_encode(root, &buf, &n) == 0) {
    rc = (sys_spine_msg_send(dest_pid, buf, (long)n, 0) == 0) ? 0 : -1;
    free(buf);
  }
  lat_free(root);
  return rc;
}

int lattice_send_message_to_pid(lattice_ctx_t *ctx, long dest_pid, const char *method, const lattice_value_t *args) {
  (void)ctx;
  return lattice_send_enveloped(dest_pid, method, args, 0, 0);
}

int lattice_send_message(lattice_ctx_t *ctx, const char *service, const char *method, const lattice_value_t *args) {
  if (!ctx || !service) return -1;
  long pid = sys_spine_service_lookup(service, 0);
  if (pid < 0) return -1;
  return lattice_send_enveloped(pid, method, args, 0, 0);
}

long lattice_send_message_with_reply(lattice_ctx_t *ctx,
                                     const char *service,
                                     const char *method,
                                     const lattice_value_t *args,
                                     lattice_reply_cb cb,
                                     void *user) {
  if (!ctx || !service) return -1;
  long pid = sys_spine_service_lookup(service, 0);
  if (pid < 0) return -1;
  if (ctx->pending_count >= (int)(sizeof(ctx->pending)/sizeof(ctx->pending[0]))) return -1;
  uint64_t id = ctx->next_id++;
  int slot = ctx->pending_count++;
  ctx->pending[slot].id = id;
  ctx->pending[slot].dest_pid = pid;
  ctx->pending[slot].cb = cb;
  ctx->pending[slot].user = user;
  ctx->pending[slot].resp = NULL;
  ctx->pending[slot].done = 0;
  if (lattice_send_enveloped(pid, method, args, 1, id) != 0) {
    // rollback slot
    ctx->pending_count--;
    return -1;
  }
  return (long)id;
}

int lattice_send_message_with_reply_sync(lattice_ctx_t *ctx,
                                         const char *service,
                                         const char *method,
                                         const lattice_value_t *args,
                                         lattice_value_t **out_resp,
                                         unsigned long timeout_ticks) {
  if (!ctx || !service || !out_resp) return -1;
  long pid = sys_spine_service_lookup(service, 0);
  if (pid < 0) {
      sys_print_str("\nlattice_send_message_with_reply_sync: service lookup failed\n");
      return -1;
  };
  if (ctx->pending_count >= (int)(sizeof(ctx->pending)/sizeof(ctx->pending[0]))) {
      sys_print_str("\nlattice_send_message_with_reply_sync: pending count exceeded\n");
      return -1;
  };
  uint64_t id = ctx->next_id++;
  int slot = ctx->pending_count++;
  ctx->pending[slot].id = id;
  ctx->pending[slot].dest_pid = pid;
  ctx->pending[slot].cb = NULL;
  ctx->pending[slot].user = NULL;
  ctx->pending[slot].resp = NULL;
  ctx->pending[slot].done = 0;
  if (lattice_send_enveloped(pid, method, args, 1, id) != 0) {
    sys_print_str("\nlattice_send_message_with_reply_sync: send failed\n");
    ctx->pending_count--;
    return -1;
  }
  while (!ctx->pending[slot].done) {
    int pr = lattice_poll(ctx, timeout_ticks);
    if (pr == 0) {
      break; // timeout
    }
    if (pr < 0) {
      break; // error
    }
  }
  if (!ctx->pending[slot].done) {
    sys_print_str("\nlattice_send_message_with_reply_sync: timeout\n");
    // timeout; cleanup slot by compacting
    for (int i = slot + 1; i < ctx->pending_count; i++) ctx->pending[i-1] = ctx->pending[i];
    ctx->pending_count--;
    return -1;
  }
  *out_resp = ctx->pending[slot].resp;
  // remove slot
  for (int i = slot + 1; i < ctx->pending_count; i++) ctx->pending[i-1] = ctx->pending[i];
  ctx->pending_count--;
  return 0;
}
