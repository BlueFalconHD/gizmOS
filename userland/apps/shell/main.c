// Minimal shell: reads keyboard input via notifications and supports `echo`.
// Reuses key mapping helpers from keynotify.
#include "../../sys/syscall.h"
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

static inline int is_space(char c) { return c == ' ' || c == '\t'; }

static inline const char *skip_spaces(const char *p) {
  while (*p && is_space(*p)) p++;
  return p;
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

  // Built-in: hello -> run HELLO.VES demo program
  rest = p;
  if (starts_with_word(p, "hello", &rest)) {
    long pid = sys_spawn("HELLO.VES", "hello");
    if (pid < 0) {
      sys_print_str("failed to spawn hello\n");
    } else {
      long status = 0;
      (void)sys_wait(&status);
    }
    sys_print_str("\n");
    print_prompt();
    return;
  }

  // Unknown command
  sys_print_str("unknown command: ");
  sys_print_str(p);
  sys_print_str("\n");
}

bool process_keypress(keypress_t *kp) {


  if (kp->type != KEYBOARD_KEY_PRESSED) {
    return false;
  }

  // Backspace handling
  if (kp->keycode == KEY_BACKSPACE) {
    if (input_len > 0) {
      input_len--;
      sys_print_str("\b \b");
    }
    return true;
  }

  // Enter (accept line). Handle both main Enter and keypad Enter.
  if (kp->keycode == KEY_ENTER || kp->keycode == KEY_KPENTER) {
    sys_print_str("\n");
    handle_command_line();
    input_len = 0;
    print_prompt();
    return true;
  }



  // Printable character
  char ch;
  if (char_from_keypress(kp, &ch) && input_len + 1 < sizeof(input_line)) {
    input_line[input_len++] = ch;
    char out[2] = {ch, '\0'};
    sys_print_str(out);
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
  print_prompt();
  sys_notif_register(1 /* keypress */, (uint64_t)&key_handler, 0, 0);
  spin();
}


