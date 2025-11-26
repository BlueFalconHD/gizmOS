// Minimal shell: reads keyboard input via notifications and supports `echo`.
// Reuses key mapping helpers from keynotify.
#include "../../sys/syscall.h"
#include "../../lattice/lattice.h"
#include "../../include/system_services.h"
#include "../keynotify/keypress.h"
#include "../keynotify/virtio_keycode.h"
#include <stdbool.h>
#include <stdint.h>

static void __attribute__((noreturn)) spin(void) {
  for (;;) {
  }
}

static inline void print_prompt(void) {
  sys_print_str("sh> ");
}

// --- Simple terminal helpers (ANSI CSI cursor movement) ---
static void csi_move(char final, unsigned int count) {
  // Emits: ESC [ <count> <final>
  // count == 0 is a no-op
  if (count == 0) return;
  char buf[16];
  // Build ESC[
  buf[0] = '\x1b';
  buf[1] = '[';
  // Convert count to decimal into tmp (reversed), then copy
  char tmp[10];
  unsigned int n = count;
  unsigned int ti = 0;
  do {
    tmp[ti++] = (char)('0' + (n % 10));
    n /= 10;
  } while (n > 0 && ti < sizeof(tmp));
  // Reverse into buf after ESC[
  unsigned int bi = 2;
  while (ti > 0 && bi < sizeof(buf) - 2) {
    buf[bi++] = tmp[--ti];
  }
  // Append final and NUL
  buf[bi++] = final;
  buf[bi] = '\0';
  sys_print_str(buf);
}

static inline void move_cursor_left(unsigned int count) { csi_move('D', count); }
static inline void move_cursor_right(unsigned int count) { csi_move('C', count); }

// Render helpers
static void print_n(const char *s, unsigned int n) {
  if (n == 0) return;
  char tmp[260];
  if (n >= sizeof(tmp)) n = sizeof(tmp) - 1;
  for (unsigned int i = 0; i < n; i++) tmp[i] = s[i];
  tmp[n] = '\0';
  sys_print_str(tmp);
}

static void print_spaces(unsigned int count) {
  if (count == 0) return;
  char tmp[260];
  if (count >= sizeof(tmp)) count = sizeof(tmp) - 1;
  for (unsigned int i = 0; i < count; i++) tmp[i] = ' ';
  tmp[count] = '\0';
  sys_print_str(tmp);
}

static inline unsigned char is_modifier(uint8_t keycode) {
  switch (keycode) {
  case KEY_LEFTCTRL:
  case KEY_LEFTSHIFT:
  case KEY_LEFTALT:
  case KEY_LEFTMETA:
  case KEY_RIGHTCTRL:
  case KEY_RIGHTSHIFT:
  case KEY_RIGHTALT:
  case KEY_RIGHTMETA:
    return 1;
  default:
    return 0;
  }
}

bool char_from_keypress(keypress_t *kp, char *out) {
  if (kp->type != KEYBOARD_KEY_PRESSED) {
    return false;
  }
  if (is_modifier(kp->keycode)) {
    return false;
  }

  const struct kc_map_entry entry = kc_map[kp->keycode];
  if (!entry.printable) {
    return false;
  }

  bool shift = keypress_lshift(kp) || keypress_rshift(kp);
  bool caps = keypress_capslock(kp);
  bool is_alpha = entry.ascii_without_caps >= 'a' && entry.ascii_without_caps <= 'z';

  bool use_shift_variant = shift;
  if (caps && is_alpha) {
    use_shift_variant = !use_shift_variant;
  }

  char ch = use_shift_variant ? entry.ascii_with_caps : entry.ascii_without_caps;
  if (ch == 0) {
    return false;
  }

  *out = ch;
  return true;
}

static char input_line[256];
static unsigned int input_len = 0;
static unsigned int cursor_pos = 0; // index in input_line, 0..input_len

static inline int is_space(char c) { return c == ' ' || c == '\t'; }

static inline const char *skip_spaces(const char *p) {
  while (*p && is_space(*p)) p++;
  return p;
}

static inline unsigned long str_len(const char *s) {
  unsigned long n = 0;
  while (s[n]) n++;
  return n;
}

static inline int starts_with_word(const char *p, const char *word, const char **rest_out) {
  const char *a = p;
  const char *b = word;
  while (*a && *b && *a == *b) {
    a++; b++;
  }
  if (*b != '\0') {
    return 0; // word not fully matched
  }
  if (*a != '\0' && !is_space(*a)) {
    return 0; // must end on boundary
  }
  if (rest_out) *rest_out = a;
  return 1;
}

static lattice_ctx_t *g_lctx = NULL;

// --- History support ---
#define HISTORY_CAP 32
static char history_lines[HISTORY_CAP][sizeof(input_line)];
static unsigned int history_count = 0; // number of valid entries (<= HISTORY_CAP)
static unsigned int history_head = 0;  // next insert position (ring)
static int history_index = -1;         // -1: not browsing; else ring index currently shown
static char pending_line_before_hist[sizeof(input_line)];
static unsigned int pending_len_before_hist = 0;

static inline unsigned int history_oldest_index(void) {
  if (history_count == 0) return 0;
  return (history_head + HISTORY_CAP - history_count) % HISTORY_CAP;
}
static inline unsigned int history_newest_index(void) {
  if (history_count == 0) return 0;
  return (history_head + HISTORY_CAP - 1) % HISTORY_CAP;
}

static void replace_current_line_with_text(const char *text) {
  // Move to beginning of input (after prompt)
  if (cursor_pos > 0) {
    move_cursor_left(cursor_pos);
  }
  // Compute new length (bounded by input buffer capacity - 1)
  unsigned int new_len = 0;
  while (text[new_len] && new_len + 1 < sizeof(input_line)) new_len++;
  // Draw new content
  print_n(text, new_len);
  // If previous line was longer, erase the tail
  if (input_len > new_len) {
    unsigned int diff = input_len - new_len;
    print_spaces(diff);
    move_cursor_left(diff);
  }
  // Copy into input buffer
  for (unsigned int i = 0; i < new_len; i++) {
    input_line[i] = text[i];
  }
  input_len = new_len;
  cursor_pos = new_len;
}

static int str_eq_local(const char *a, const char *b) {
  unsigned int i = 0;
  while (a[i] && b[i]) {
    if (a[i] != b[i]) return 0;
    i++;
  }
  return a[i] == '\0' && b[i] == '\0';
}

static int is_all_space_local(const char *s, unsigned int n) {
  for (unsigned int i = 0; i < n; i++) {
    if (!is_space(s[i])) return 0;
  }
  return n > 0 ? 0 : 1;
}

static void history_maybe_push_current(void) {
  // Ensure input_line is NUL-terminated
  if (input_len >= sizeof(input_line)) return;
  input_line[input_len] = '\0';
  // Skip pushing empty or whitespace-only lines
  const char *p = input_line;
  while (*p && is_space(*p)) p++;
  if (*p == '\0') return;
  // Avoid consecutive duplicates
  if (history_count > 0) {
    unsigned int newest = history_newest_index();
    // Compare with newest entry
    if (str_eq_local(history_lines[newest], input_line)) {
      return;
    }
  }
  // Insert at head
  unsigned int idx = history_head;
  // Copy bounded
  unsigned int i = 0;
  while (input_line[i] && i + 1 < sizeof(history_lines[0])) {
    history_lines[idx][i] = input_line[i];
    i++;
  }
  history_lines[idx][i] = '\0';
  // Advance head
  history_head = (history_head + 1) % HISTORY_CAP;
  if (history_count < HISTORY_CAP) {
    history_count++;
  }
}

static void begin_history_browse_if_needed(void) {
  if (history_index == -1) {
    // Save the current editing line so it can be restored when browsing ends
    if (input_len + 1 < sizeof(pending_line_before_hist)) {
      for (unsigned int i = 0; i < input_len; i++) pending_line_before_hist[i] = input_line[i];
      pending_line_before_hist[input_len] = '\0';
      pending_len_before_hist = input_len;
    } else {
      pending_line_before_hist[0] = '\0';
      pending_len_before_hist = 0;
    }
  }
}

static void find_and_run(const char *cmdline) {
  if (!g_lctx) return;
  // Tokenize: program name + up to 8 args, separated by spaces
  const int MAX_ARGS = 8;
  const char *argv[MAX_ARGS];
  int argc = 0;
  // Extract program token
  const char *p = skip_spaces(cmdline);
  const char *start = p;
  while (*p && !is_space(*p)) p++;
  if (p == start) return;
  char namebuf[64];
  unsigned int nlen = 0;
  const char *q = start;
  while (q < p && nlen + 1 < sizeof(namebuf)) namebuf[nlen++] = *q++;
  namebuf[nlen] = '\0';
  // Gather args
  const char *rest = skip_spaces(p);
  while (*rest && argc < MAX_ARGS) {
    const char *as = rest;
    while (*rest && !is_space(*rest)) rest++;
    argv[argc++] = as;
    if (*rest == '\0') break;
    *((char *)rest) = '\0'; // temporarily null-terminate in-place on input_line buffer
    rest = skip_spaces(rest + 1);
  }

  lattice_value_t *req = lat_map_new();
  lat_map_set_str(req, "name", namebuf);
  lattice_value_t *resp = NULL;
  if (lattice_send_message_with_reply_sync(g_lctx, SERVICE_PATHD, "pathd_locate", req, &resp, 100000) == 0 && resp) {
    int ok = 0;
    (void)lat_map_get_bool(resp, "ok", &ok);

    if (ok) {
      const char *path = NULL;
      lat_map_get_str(resp, "path", &path);

      if (path) {
        long pid = sys_spawn2(path, namebuf, (const char *const *)argv, argc);
        if (pid >= 0) {
          long status = 0;
          (void)sys_wait(&status);
        } else {
          sys_print_str("failed to spawn ");
          sys_print_str(path);
          sys_print_str("\n");
        }
        lat_free(resp);
        lat_free(req);
        return;
      }
    }
    lat_free(resp);
  }
  lat_free(req);
  sys_print_str("unknown command: ");
  sys_print_str(namebuf);
  sys_print_str("\n");
}

static void handle_command_line(void) {
  input_line[input_len] = '\0';

  const char *p = skip_spaces(input_line);
  if (*p == '\0') {
    return; // empty line
  }

  const char *rest = p;
  if (starts_with_word(p, "echo", &rest)) {
    rest = skip_spaces(rest);
    // Print the rest verbatim
    // Temporarily ensure null-termination (already is) and print
    sys_print_str(rest);
    sys_print_str("\n");
    return;
  }

  // Built-in: send <service> <text...> -> Spine message to service
  rest = p;
  if (starts_with_word(p, "send", &rest)) {
    rest = skip_spaces(rest);
    // Extract service token
    const char *svc = rest;
    unsigned int svc_len = 0;
    while (rest[svc_len] && !is_space(rest[svc_len])) svc_len++;
    if (svc_len == 0) {
      sys_print_str("usage: send <service> <text>\n");
      return;
    }
    char svcbuf[17];
    if (svc_len > 16) svc_len = 16;
    for (unsigned int i = 0; i < svc_len; i++) svcbuf[i] = svc[i];
    svcbuf[svc_len] = '\0';
    const char *msg = skip_spaces(rest + svc_len);
    long pid = sys_spine_service_lookup(svcbuf, 0);
    if (pid < 0) {
      sys_print_str("service not found: ");
      sys_print_str(svcbuf);
      sys_print_str("\n");
      return;
    }
    unsigned long n = str_len(msg);
    long rc = sys_spine_msg_send(pid, msg, (long)n, 0);
    (void)rc;
    sys_print_str("\n");
    return;
  }

  // Built-in: hello -> run HELLO.VES demo program
  rest = p;
  if (starts_with_word(p, "hello", &rest)) {
    long pid = sys_spawn("/vessels/hello.vessel", "hello");
    if (pid < 0) {
      sys_print_str("failed to spawn hello\n");
    } else {
      long status = 0;
      (void)sys_wait(&status);
    }
    sys_print_str("\n");
    return;
  }

  find_and_run(p);
}

bool process_keypress(keypress_t *kp) {


  if (kp->type != KEYBOARD_KEY_PRESSED) {
    return false;
  }

  // Arrow navigation and editing controls
  const bool ctrl_down = keypress_lctrl(kp) || keypress_rctrl(kp);
  const bool meta_down = keypress_lmeta(kp) || keypress_rmeta(kp);
  const unsigned int accel = 5; // ctrl+arrows: accelerated navigation step

  // Left/Right/Home/End/Delete support
  if (kp->keycode == KEY_LEFT) {
    unsigned int max_left = cursor_pos;
    unsigned int step = 1;
    if (meta_down) {
      step = max_left;
    } else if (ctrl_down) {
      step = (max_left < accel) ? max_left : accel;
    }
    if (step > 0) {
      move_cursor_left(step);
      cursor_pos -= step;
    }
    return true;
  }

  if (kp->keycode == KEY_RIGHT) {
    unsigned int available = (input_len > cursor_pos) ? (input_len - cursor_pos) : 0;
    unsigned int step = 1;
    if (meta_down) {
      step = available;
    } else if (ctrl_down) {
      step = (available < accel) ? available : accel;
    }
    if (step > 0) {
      move_cursor_right(step);
      cursor_pos += step;
    }
    return true;
  }

  if (kp->keycode == KEY_HOME) {
    if (cursor_pos > 0) {
      move_cursor_left(cursor_pos);
      cursor_pos = 0;
    }
    return true;
  }

  if (kp->keycode == KEY_END) {
    if (cursor_pos < input_len) {
      unsigned int step = input_len - cursor_pos;
      move_cursor_right(step);
      cursor_pos = input_len;
    }
    return true;
  }

  // History navigation: Up/Down arrows
  if (kp->keycode == KEY_UP) {
    if (history_count == 0) return true;
    begin_history_browse_if_needed();
    unsigned int newest = history_newest_index();
    unsigned int oldest = history_oldest_index();
    if (history_index == -1) {
      history_index = (int)newest;
    } else if ((unsigned int)history_index != oldest) {
      history_index = (history_index + HISTORY_CAP - 1) % HISTORY_CAP;
    }
    replace_current_line_with_text(history_lines[history_index]);
    return true;
  }

  if (kp->keycode == KEY_DOWN) {
    if (history_count == 0) return true;
    if (history_index == -1) {
      // Not browsing, nothing to do
      return true;
    }
    unsigned int newest = history_newest_index();
    if ((unsigned int)history_index == newest) {
      // Leave history browsing, restore pending edit line
      history_index = -1;
      replace_current_line_with_text(pending_line_before_hist);
    } else {
      history_index = (history_index + 1) % HISTORY_CAP;
      replace_current_line_with_text(history_lines[history_index]);
    }
    return true;
  }

  if (kp->keycode == KEY_DELETE) {
    if (cursor_pos < input_len) {
      // Delete char at cursor_pos (shift left tail)
      unsigned int tail_len = (input_len - cursor_pos - 1);
      for (unsigned int i = cursor_pos; i + 1 < input_len; i++) {
        input_line[i] = input_line[i + 1];
      }
      input_len--;
      // Redraw tail and a blank, then reposition
      print_n(&input_line[cursor_pos], tail_len);
      sys_print_str(" ");
      move_cursor_left(tail_len + 1);
      history_index = -1; // reset browsing on edit
    }
    return true;
  }

  // Backspace handling
  if (kp->keycode == KEY_BACKSPACE) {
    if (cursor_pos > 0 && input_len > 0) {
      if (meta_down) {
        // Delete everything before cursor; keep tail
        unsigned int old_pos = cursor_pos;
        unsigned int tail_len = input_len - old_pos;
        // Move cursor to start
        move_cursor_left(old_pos);
        // Shift tail to the beginning
        for (unsigned int i = 0; i < tail_len; i++) {
          input_line[i] = input_line[old_pos + i];
        }
        input_len = tail_len;
        cursor_pos = 0;
        // Redraw new line content and clear leftover area
        print_n(&input_line[0], tail_len);
        print_spaces(old_pos);
        // Return cursor to the start of the line
        move_cursor_left(tail_len + old_pos);
      } else {
        // Move cursor left one
        move_cursor_left(1);
        // Remove char before cursor_pos
        unsigned int tail_len = input_len - cursor_pos;
        for (unsigned int i = cursor_pos - 1; i + 1 < input_len; i++) {
          input_line[i] = input_line[i + 1];
        }
        cursor_pos--;
        input_len--;
        // Redraw tail (now starts at new cursor_pos), add a blank to erase leftover
        print_n(&input_line[cursor_pos], tail_len);
        sys_print_str(" ");
        // Return cursor to logical position
        move_cursor_left(tail_len + 1);
      }
      history_index = -1; // reset browsing on edit
    }
    return true;
  }

  // Enter (accept line). Handle both main Enter and keypad Enter.
  if (kp->keycode == KEY_ENTER || kp->keycode == KEY_KPENTER) {
    // Push to history before command handling mutates the buffer
    history_maybe_push_current();
    sys_print_str("\n");
    handle_command_line();
    input_len = 0;
    cursor_pos = 0;
    history_index = -1;
    print_prompt();
    return true;
  }



  // Printable character
  char ch;
  if (char_from_keypress(kp, &ch) && input_len + 1 < sizeof(input_line)) {
    if (cursor_pos == input_len) {
      // Append at end
      input_line[input_len++] = ch;
      cursor_pos++;
      char out[2] = {ch, '\0'};
      sys_print_str(out);
    } else {
      // Insert at cursor: shift right and redraw tail
      unsigned int tail_len = input_len - cursor_pos;
      // Shift right by one
      for (unsigned int i = input_len; i > cursor_pos; i--) {
        input_line[i] = input_line[i - 1];
      }
      input_line[cursor_pos] = ch;
      input_len++;
      cursor_pos++;
      // Redraw from inserted position (inserted char + tail)
      print_n(&input_line[cursor_pos - 1], tail_len + 1);
      // Move back over the tail
      move_cursor_left(tail_len);
    }
    history_index = -1; // reset browsing on edit
    return true;
  }

  return false;
}

static void
key_handler(uint64_t type, uint64_t payload_uva, uint64_t len, uint64_t arg) {
  (void)type;
  (void)arg;

  if (len >= sizeof(keypress_t)) {
    keypress_t *kp = (keypress_t *)payload_uva;
    process_keypress(kp);
  }
}

int main(void) {
  g_lctx = lattice_init(NULL);
  print_prompt();
  sys_notif_register(1 /* keypress */, (uint64_t)&key_handler, 0, 0);
  spin();
}
