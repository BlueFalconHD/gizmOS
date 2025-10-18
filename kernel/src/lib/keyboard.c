#include "keyboard.h"
#include "lib/debug.h"
#include <lib/log.h>

#define KBD_DEBUG 1

static inline log_t *kbd_log() {
  static log_t *l = NULL;
  if (!l) {
    l = g_log_create("input", "kbd");
    #if KBD_DEBUG
    g_log_set_level(l, LOG_LEVEL_DEBUG);
    #else
    g_log_set_level(l, LOG_LEVEL_INFO);
    #endif
  }
  return l;
}
#include <lib/kalloc.h>
#include <lib/str.h>

RESULT_TYPE(keypress_t *)
make_keypress(uint8_t keycode, uint8_t modifiers, keypress_type_t type) {
  keypress_t *kp = (keypress_t *)kalloc(sizeof(keypress_t));
  if (!kp) {
    dbg("kalloc(...) == NULL");
    return RESULT_FAILURE(RESULT_NOMEM);
  }
  kp->keycode = keycode;
  kp->modifiers = modifiers;
  kp->type = type;
  return RESULT_SUCCESS(kp);
}

void keypress_debug(keypress_t *kp) {
  if (!kp) {
    dbg("kp == NULL");
    return;
  }

  char *modifiers_str = (char *)kalloc(128);
  if (!modifiers_str) {
    dbg("modifiers_str == NULL");
    return;
  }

  modifiers_str[0] = '\0'; // Initialize the string

  if (keypress_rshift(kp)) {
    strcat(modifiers_str, "RSHIFT,");
  }

  if (keypress_lshift(kp)) {
    strcat(modifiers_str, "LSHIFT,");
  }

  if (keypress_lctrl(kp)) {
    strcat(modifiers_str, "LCTRL,");
  }

  if (keypress_rctrl(kp)) {
    strcat(modifiers_str, "RCTRL,");
  }

  if (keypress_lalt(kp)) {
    strcat(modifiers_str, "LALT,");
  }

  if (keypress_ralt(kp)) {
    strcat(modifiers_str, "RALT,");
  }

  if (keypress_lmeta(kp)) {
    strcat(modifiers_str, "LMETA,");
  }

  if (keypress_rmeta(kp)) {
    strcat(modifiers_str, "RMETA,");
  }

  if (keypress_capslock(kp)) {
    strcat(modifiers_str, "CAPSLOCK,");
  }

  LOG_DEBUG(kbd_log(), "<keypress:%{type: hex} keycode=%{type: hex} mods=%{type: hex} [%{type: str}] type=%{type: int}>", (uint64_t)kp, kp->keycode, (uint64_t)kp->modifiers, modifiers_str, kp->type);

  kfree(modifiers_str);
}
