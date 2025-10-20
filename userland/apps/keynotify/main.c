#include "../../sys/syscall.h"
#include "keypress.h"
#include "virtio_keycode.h"
#include <stdbool.h>
#include <stdint.h>

static void __attribute__((noreturn)) spin(void) {
  for (;;) {
  }
}

unsigned char is_modifier(uint8_t keycode) {
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

static inline unsigned char ascii_upper_to_lower(unsigned char c) {
  if (c >= 'A' && c <= 'Z') {
    return c + ('a' - 'A');
  }
  return c;
}

static inline unsigned char ascii_lower_to_upper(unsigned char c) {
  if (c >= 'a' && c <= 'z') {
    return c - ('a' - 'A');
  }
  return c;
}

void simple_text_editor_handle_keycode(keypress_t *kp) {
  if (is_modifier(kp->keycode) || kp->type != KEYBOARD_KEY_PRESSED) {
    return;
  }

  if (kp->keycode == KEY_BACKSPACE) {
    sys_print_str("\b \b"); // move back, overwrite, move back again
    return;
  }

  const struct kc_map_entry entry = kc_map[kp->keycode];
  if (!entry.printable && entry.ascii_without_caps == 0) {
    sys_print_str(virtio_keycode_to_string(kp->keycode));
    return;
  }

  bool shift = keypress_lshift(kp) || keypress_rshift(kp);
  bool caps = keypress_capslock(kp);
  bool is_alpha =
      entry.ascii_without_caps >= 'a' && entry.ascii_without_caps <= 'z';

  bool use_shift_variant = shift;
  if (caps && is_alpha) {
    use_shift_variant = !use_shift_variant;
  }

  char ch =
      use_shift_variant ? entry.ascii_with_caps : entry.ascii_without_caps;
  char out[2] = {ch, '\0'};
  sys_print_str(out);
}

static void __attribute__((noreturn))
key_handler(uint64_t type, uint64_t payload_uva, uint64_t len, uint64_t arg) {
  (void)type;
  (void)arg;
  if (len >= 4) {
    keypress_t *kp = (keypress_t *)payload_uva;

    simple_text_editor_handle_keycode(kp);
  }

  sys_notif_done();
  __builtin_unreachable();
}

int main(void) {
  sys_notif_register(1 /* keypress */, (uint64_t)&key_handler, 0, 0);
  spin();
}
