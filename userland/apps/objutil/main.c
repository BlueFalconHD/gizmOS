#include "../../sys/syscall.h"
#include "../../libc-lite/libc.h"
#include <stdint.h>
#include <stddef.h>
/*
  ObjectFS utility CLI
  Commands:
    - ls <path>
    - stat <path>
    - attrs <path>
    - getattr <path> <key>
    - setattr <path> <str|int|bool> <key> <value>
    - cat <path> [offset] [n]
    - read <path> <offset> <n>    (hex)
    - write <path> <offset> <data...>  (write string bytes)
    - create <dir> <name>         (capabilities inferred on first use)
    - link <dir> <name> <target_path>
    - unlink <dir> <name>
    - rename <dir> <old> <new>
*/

static int streq(const char *a, const char *b) {
  size_t i = 0;
  while (a[i] && b[i]) { if (a[i] != b[i]) return 0; i++; }
  return a[i] == '\0' && b[i] == '\0';
}

static int str_to_int(const char *s, long *out) {
  if (!s || !s[0]) return 0;
  long sign = 1;
  size_t i = 0;
  if (s[0] == '-') { sign = -1; i++; }
  long v = 0;
  for (; s[i]; i++) {
    char c = s[i];
    if (c < '0' || c > '9') return 0;
    v = v * 10 + (c - '0');
  }
  *out = sign * v;
  return 1;
}

static void print_str(const char *s) { sys_print_str(s ? s : ""); }
static void println(const char *s) { sys_print_str(s ? s : ""); sys_print_str("\n"); }

static void print_dec_unsigned(unsigned long v) {
  char buf[21];
  int i = 0;
  if (v == 0) { sys_print_str("0"); return; }
  while (v > 0 && i < (int)sizeof(buf)) {
    buf[i++] = (char)('0' + (v % 10));
    v /= 10;
  }
  while (i > 0) { char c = buf[--i]; sys_print_str((char[]){c,0}); }
}

static void print_hex_u64(uint64_t x) {
  const char *hex = "0123456789abcdef";
  char out[2 + 16 + 1];
  out[0] = '0'; out[1] = 'x';
  for (int i = 0; i < 16; i++) {
    int shift = (15 - i) * 4;
    out[2 + i] = hex[(x >> shift) & 0xF];
  }
  out[18] = '\n';
  out[18] = '\0';
  sys_print_str(out);
}

static int cmd_ls(const char *path) {
  long h = sys_objh_open_at(path, 0);
  if (h < 0) { sys_print_str("open failed\n"); return 1; }
  sys_objdirent_t ents[64];
  long n = sys_objh_list_subobjects(h, ents, (long)sizeof(ents));
  if (n < 0) { sys_print_str("list failed\n"); sys_objh_close(h); return 1; }
  long cnt = n / (long)sizeof(sys_objdirent_t);
  for (long i = 0; i < cnt; i++) {
    sys_objdirent_t *e = &ents[i];
    // print: <type> <id> <name>
    if (e->type == 1) sys_print_str("f ");
    else if (e->type == 2) sys_print_str("d ");
    else if (e->type == 3) sys_print_str("r ");
    else sys_print_str("? ");
    print_dec_unsigned((unsigned long)e->id);
    sys_print_str(" ");
    sys_print_str(e->name);
    sys_print_str("\n");
  }
  sys_objh_close(h);
  return 0;
}

static int cmd_stat(const char *path) {
  long h = sys_objh_open_at(path, 0);
  if (h < 0) { sys_print_str("open failed\n"); return 1; }
  sys_obj_desc_t d;
  if (sys_objh_desc(h, &d) != 0) { sys_print_str("desc failed\n"); sys_objh_close(h); return 1; }
  sys_print_str("id: "); print_dec_unsigned((unsigned long)d.id); sys_print_str("\n");
  sys_print_str("kind: "); print_dec_unsigned((unsigned long)d.kind); sys_print_str("\n");
  sys_print_str("size: "); print_dec_unsigned((unsigned long)d.size); sys_print_str("\n");
  sys_print_str("nlink: "); print_dec_unsigned((unsigned long)d.nlink); sys_print_str("\n");
  if (d.kind == 3) { sys_print_str("target_id: "); print_dec_unsigned((unsigned long)d.target_id); sys_print_str("\n"); }
  sys_objh_close(h);
  return 0;
}

static int cmd_attrs(const char *path) {
  long h = sys_objh_open_at(path, 0);
  if (h < 0) { sys_print_str("open failed\n"); return 1; }
  unsigned char buf[ sizeof(sys_objattr_t) * 32 ];
  long n = sys_objh_attr_list(h, buf, (long)sizeof(buf));
  if (n < 0) { sys_print_str("attr_list failed\n"); sys_objh_close(h); return 1; }
  long cnt = n / (long)sizeof(sys_objattr_t);
  for (long i = 0; i < cnt; i++) {
    sys_objattr_t *a = &((sys_objattr_t *)buf)[i];
    sys_print_str("- ");
    sys_print_str(a->key);
    sys_print_str(" (");
    if (a->type == 0) sys_print_str("str");
    else if (a->type == 1) sys_print_str("int");
    else if (a->type == 2) sys_print_str("bool");
    else sys_print_str("?");
    sys_print_str(")\n");
  }
  sys_objh_close(h);
  return 0;
}

static int cmd_getattr(const char *path, const char *key) {
  long h = sys_objh_open_at(path, 0);
  if (h < 0) { sys_print_str("open failed\n"); return 1; }
  sys_objattr_t av;
  char strbuf[256];
  long rc = sys_objh_attr_get(h, key, &av, strbuf, (long)sizeof(strbuf));
  if (rc < 0) { sys_print_str("attr_get failed\n"); sys_objh_close(h); return 1; }
  if (av.type == 0) {
    sys_print_str(key); sys_print_str("=\""); sys_print_str(strbuf); sys_print_str("\"\n");
  } else if (av.type == 1) {
    sys_print_str(key); sys_print_str("="); print_dec_unsigned((unsigned long)rc); sys_print_str(" (int)\n");
  } else if (av.type == 2) {
    sys_print_str(key); sys_print_str("="); sys_print_str(rc ? "true" : "false"); sys_print_str("\n");
  } else {
    sys_print_str("unknown attr type\n");
  }
  sys_objh_close(h);
  return 0;
}

static int cmd_setattr(const char *path, const char *type, const char *key, const char *value) {
  long h = sys_objh_open_at(path, 0);
  if (h < 0) { sys_print_str("open failed\n"); return 1; }
  int ok = 0;
  if (streq(type, "str")) {
    ok = (sys_objh_set_attr(h, key, 0, value, (long)(strlen(value) + 1)) == 0);
  } else if (streq(type, "int")) {
    long v = 0; if (!str_to_int(value, &v)) { sys_print_str("bad int\n"); sys_objh_close(h); return 1; }
    ok = (sys_objh_set_attr(h, key, 1, &v, (long)sizeof(v)) == 0);
  } else if (streq(type, "bool")) {
    int b = (streq(value, "1") || streq(value, "true")) ? 1 : 0;
    unsigned char vb = (unsigned char)(b ? 1 : 0);
    ok = (sys_objh_set_attr(h, key, 2, &vb, (long)sizeof(vb)) == 0);
  } else {
    sys_print_str("type must be str|int|bool\n"); sys_objh_close(h); return 1;
  }
  sys_print_str(ok ? "ok\n" : "failed\n");
  sys_objh_close(h);
  return ok ? 0 : 1;
}

static int cmd_cat(const char *path, long opt_off, long opt_n, int has_off, int has_n) {
  long h = sys_objh_open_at(path, 0);
  if (h < 0) { sys_print_str("open failed\n"); return 1; }
  sys_obj_desc_t d;
  if (sys_objh_desc(h, &d) != 0) { sys_print_str("desc failed\n"); sys_objh_close(h); return 1; }
  unsigned long offset = has_off ? (unsigned long)opt_off : 0;
  unsigned long remain = has_n ? (unsigned long)opt_n : (unsigned long)((d.size > offset) ? (d.size - offset) : 0);
  char buf[256];
  while (remain > 0) {
    long chunk = (remain > sizeof(buf)) ? (long)sizeof(buf) : (long)remain;
    long r = sys_objh_read(h, buf, offset, chunk);
    if (r <= 0) break;
    // ensure NUL-termination before printing
    if (r >= (long)sizeof(buf)) r = (long)sizeof(buf) - 1;
    buf[r] = '\0';
    sys_print_str(buf);
    offset += (unsigned long)r;
    remain -= (unsigned long)r;
  }
  sys_objh_close(h);
  sys_print_str("\n");
  return 0;
}

static void print_hex_line(const unsigned char *buf, long n, unsigned long base_off) {
  const char *hex = "0123456789abcdef";
  // offset
  char offbuf[11]; // 8 hex + ": "
  for (int i = 0; i < 8; i++) {
    int shift = (7 - i) * 4;
    offbuf[i] = hex[(base_off >> shift) & 0xF];
  }
  offbuf[8] = ':'; offbuf[9] = ' '; offbuf[10] = '\0';
  sys_print_str(offbuf);
  // bytes
  for (long i = 0; i < n; i++) {
    char b[4];
    b[0] = hex[(buf[i] >> 4) & 0xF];
    b[1] = hex[buf[i] & 0xF];
    b[2] = ' ';
    b[3] = '\0';
    sys_print_str(b);
  }
  sys_print_str("\n");
}

static int cmd_read(const char *path, long off, long n) {
  long h = sys_objh_open_at(path, 0);
  if (h < 0) { sys_print_str("open failed\n"); return 1; }
  unsigned char buf[32];
  unsigned long offset = (unsigned long)off;
  long remain = n;
  while (remain > 0) {
    long want = (remain > (long)sizeof(buf)) ? (long)sizeof(buf) : remain;
    long r = sys_objh_read(h, buf, offset, want);
    if (r <= 0) break;
    print_hex_line(buf, r, offset);
    offset += (unsigned long)r;
    remain -= r;
  }
  sys_objh_close(h);
  return 0;
}

static int cmd_write(const char *path, long off, int argc, char **argv, int argi) {
  long h = sys_objh_open_at(path, 0);
  if (h < 0) { sys_print_str("open failed\n"); return 1; }
  // Concatenate remaining args with spaces
  char buf[512];
  long pos = 0;
  for (int i = argi; i < argc; i++) {
    const char *s = argv[i];
    size_t len = strlen(s);
    if (pos + (long)len + 1 >= (long)sizeof(buf)) break;
    for (size_t j = 0; j < len; j++) buf[pos++] = s[j];
    if (i + 1 < argc) buf[pos++] = ' ';
  }
  long w = sys_objh_write(h, buf, (unsigned long)off, pos);
  sys_objh_close(h);
  if (w < 0) { sys_print_str("write failed\n"); return 1; }
  sys_print_str("wrote "); print_dec_unsigned((unsigned long)w); sys_print_str(" bytes\n");
  return 0;
}

static int cmd_create(const char *dir, const char *name) {
  long dh = sys_objh_open_at(dir, 0);
  if (dh < 0) { sys_print_str("open dir failed\n"); return 1; }
  long nh = sys_objh_create(dh, name, 0, 0);
  sys_objh_close(dh);
  if (nh < 0) { sys_print_str("create failed\n"); return 1; }
  sys_objh_close(nh);
  sys_print_str("ok\n");
  return 0;
}

static int cmd_link(const char *dir, const char *name, const char *target_path) {
  long dh = sys_objh_open_at(dir, 0);
  if (dh < 0) { sys_print_str("open dir failed\n"); return 1; }
  long th = sys_objh_open_at(target_path, 0);
  if (th < 0) { sys_objh_close(dh); sys_print_str("open target failed\n"); return 1; }
  long rc = sys_objh_link(dh, name, th);
  sys_objh_close(th);
  sys_objh_close(dh);
  if (rc != 0) { sys_print_str("link failed\n"); return 1; }
  sys_print_str("ok\n");
  return 0;
}

static int cmd_unlink(const char *dir, const char *name) {
  long dh = sys_objh_open_at(dir, 0);
  if (dh < 0) { sys_print_str("open dir failed\n"); return 1; }
  long rc = sys_objh_unlink(dh, name);
  sys_objh_close(dh);
  if (rc != 0) { sys_print_str("unlink failed\n"); return 1; }
  sys_print_str("ok\n");
  return 0;
}

static int cmd_rename(const char *dir, const char *oldn, const char *newn) {
  long dh = sys_objh_open_at(dir, 0);
  if (dh < 0) { sys_print_str("open dir failed\n"); return 1; }
  long rc = sys_objh_rename(dh, oldn, newn);
  sys_objh_close(dh);
  if (rc != 0) { sys_print_str("rename failed\n"); return 1; }
  sys_print_str("ok\n");
  return 0;
}

static void usage() {
  #define pl(x) sys_print_str(x)

  pl("Usage:\n");
  pl("  objutil ls <path>\n");
  pl("  objutil stat <path>\n");
  pl("  objutil attrs <path>\n");
  pl("  objutil getattr <path> <key>\n");
  pl("  objutil setattr <path> <str|int|bool> <key> <value>\n");
  pl("  objutil cat <path> [offset] [n]\n");
  pl("  objutil read <path> <offset> <n>\n");
  pl("  objutil write <path> <offset> <data>\n");
  pl("  objutil create <dir> <name>\n");
  pl("  objutil link <dir> <name> <target_path>\n");
  pl("  objutil unlink <dir> <name>\n");
  pl("  objutil rename <dir> <old> <new>\n");

  #undef pl
}

int main(int argc, char **argv) {
  if (argc < 2) { usage(); return 1; }
  const char *cmd = argv[1];
  if (streq(cmd, "ls")) {
    if (argc < 3) { usage(); return 1; }
    return cmd_ls(argv[2]);
  } else if (streq(cmd, "stat")) {
    if (argc < 3) { usage(); return 1; }
    return cmd_stat(argv[2]);
  } else if (streq(cmd, "attrs")) {
    if (argc < 3) { usage(); return 1; }
    return cmd_attrs(argv[2]);
  } else if (streq(cmd, "getattr")) {
    if (argc < 4) { usage(); return 1; }
    return cmd_getattr(argv[2], argv[3]);
  } else if (streq(cmd, "setattr")) {
    if (argc < 6) { usage(); return 1; }
    return cmd_setattr(argv[2], argv[3], argv[4], argv[5]);
  } else if (streq(cmd, "cat")) {
    if (argc < 3) { usage(); return 1; }
    long off=0, n=0; int has_off=0, has_n=0;
    if (argc >= 4) { if (!str_to_int(argv[3], &off)) { sys_print_str("bad offset\n"); return 1; } has_off=1; }
    if (argc >= 5) { if (!str_to_int(argv[4], &n)) { sys_print_str("bad n\n"); return 1; } has_n=1; }
    return cmd_cat(argv[2], off, n, has_off, has_n);
  } else if (streq(cmd, "read")) {
    if (argc < 5) { usage(); return 1; }
    long off=0, n=0;
    if (!str_to_int(argv[3], &off) || !str_to_int(argv[4], &n)) { sys_print_str("bad off/n\n"); return 1; }
    return cmd_read(argv[2], off, n);
  } else if (streq(cmd, "write")) {
    if (argc < 5) { usage(); return 1; }
    long off=0; if (!str_to_int(argv[3], &off)) { sys_print_str("bad off\n"); return 1; }
    return cmd_write(argv[2], off, argc, argv, 4);
  } else if (streq(cmd, "create")) {
    if (argc < 4) { usage(); return 1; }
    return cmd_create(argv[2], argv[3]);
  } else if (streq(cmd, "link")) {
    if (argc < 5) { usage(); return 1; }
    return cmd_link(argv[2], argv[3], argv[4]);
  } else if (streq(cmd, "unlink")) {
    if (argc < 4) { usage(); return 1; }
    return cmd_unlink(argv[2], argv[3]);
  } else if (streq(cmd, "rename")) {
    if (argc < 5) { usage(); return 1; }
    return cmd_rename(argv[2], argv[3], argv[4]);
  } else {
    usage();
    return 1;
  }
}
