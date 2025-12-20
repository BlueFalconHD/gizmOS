#pragma once

#include "../libc-lite/libc.h"
#include "../sys/syscall.h"
#include <stdint.h>
#include <stddef.h>

// ---------- Lattice types ----------

typedef enum {
  LAT_T_NULL = 0,
  LAT_T_BOOL = 1,
  LAT_T_INT  = 2,
  LAT_T_STR  = 3,
  LAT_T_MAP  = 4,
  LAT_T_ARR  = 5,
} lattice_type_t;

typedef struct lattice_value lattice_value_t;

typedef struct {
  char            *key;   // heap-allocated, NUL-terminated
  lattice_value_t *value; // owned by map
} lattice_map_entry_t;

typedef struct {
  lattice_map_entry_t *entries;
  size_t count;
  size_t capacity;
} lattice_map_t;

typedef struct {
  lattice_value_t **items;
  size_t count;
  size_t capacity;
} lattice_array_t;

struct lattice_value {
  lattice_type_t type;
  union {
    int64_t          i64;
    int              b;
    char            *str;  // heap-allocated; length is strlen(str)
    lattice_map_t    map;
    lattice_array_t  arr;
  } as;
};

// ---------- Value builders / accessors ----------

lattice_value_t *lat_null();
lattice_value_t *lat_bool(int b);
lattice_value_t *lat_int(int64_t v);
lattice_value_t *lat_str(const char *s); // duplicates string
lattice_value_t *lat_map_new();
lattice_value_t *lat_array_new();

void lat_free(lattice_value_t *v);

// Map helpers (steals ownership of v)
void lat_map_set(lattice_value_t *map, const char *key, lattice_value_t *v);
// Convenience setters (copy input)
void lat_map_set_str(lattice_value_t *map, const char *key, const char *s);
void lat_map_set_int(lattice_value_t *map, const char *key, int64_t v);
void lat_map_set_bool(lattice_value_t *map, const char *key, int b);

// Getters (do not transfer ownership)
lattice_value_t *lat_map_get(const lattice_value_t *map, const char *key);
int              lat_map_get_str(const lattice_value_t *map, const char *key, const char **out_str);
int              lat_map_get_int(const lattice_value_t *map, const char *key, int64_t *out_v);
int              lat_map_get_bool(const lattice_value_t *map, const char *key, int *out_b);

// Array helpers (steals ownership of v)
void lat_array_push(lattice_value_t *arr, lattice_value_t *v);

// ---------- Encoding ----------
// Binary TLV (little-endian) encoder/decoder for Lattice values
// Produces heap buffer; caller must free(*out_buf).
int lat_encode(const lattice_value_t *v, void **out_buf, unsigned long *out_len);
int lat_decode(const void *buf, unsigned long len, lattice_value_t **out_v);

// ---------- Lattice IPC (Spine-based) ----------
typedef struct lattice_ctx lattice_ctx_t;

typedef void (*lattice_method_cb)(lattice_ctx_t *ctx,
                                  const lattice_value_t *req,
                                  lattice_value_t **resp_out,
                                  void *user);

typedef void (*lattice_reply_cb)(lattice_ctx_t *ctx,
                                 const lattice_value_t *resp,
                                 void *user);

// Initialize and (optionally) advertise a service name.
// Returns an opaque context pointer or NULL on failure.
lattice_ctx_t *lattice_init(const char *service_name);

// Block waiting for one incoming Spine message, then dispatch it (request/response).
// Returns:
// - 1 if a message was processed
// - 0 on timeout
// - -1 on failure
int lattice_poll(lattice_ctx_t *ctx, unsigned long timeout_ticks);

// Register a method handler for incoming requests.
int lattice_register(lattice_ctx_t *ctx, const char *method, lattice_method_cb cb, void *user);

// Fire-and-forget message to a service (by name) or direct PID.
// Returns 0 on success, -1 on failure.
int lattice_send_message(lattice_ctx_t *ctx, const char *service, const char *method, const lattice_value_t *args);
int lattice_send_message_to_pid(lattice_ctx_t *ctx, long dest_pid, const char *method, const lattice_value_t *args);

// With reply (async) - returns req_id or -1 on failure.
long lattice_send_message_with_reply(lattice_ctx_t *ctx,
                                     const char *service,
                                     const char *method,
                                     const lattice_value_t *args,
                                     lattice_reply_cb cb,
                                     void *user);

// With reply (sync) - waits (busy-yield) until response or timeout_ticks==0 (no timeout).
// Returns 0 on success and sets *out_resp (caller must lat_free), -1 on failure/timeout.
int lattice_send_message_with_reply_sync(lattice_ctx_t *ctx,
                                         const char *service,
                                         const char *method,
                                         const lattice_value_t *args,
                                         lattice_value_t **out_resp,
                                         unsigned long timeout_ticks);

