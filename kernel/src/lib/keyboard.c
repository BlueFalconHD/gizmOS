#include "keyboard.h"
#include "lib/print.h"
#include <lib/str.h>

RESULT_TYPE(keypress_t *)
make_keypress(uint8_t keycode, uint8_t modifiers, keypress_type_t type) {
  keypress_t *kp = (keypress_t *)alloc_page();
  if (!kp) {
    return RESULT_FAILURE(RESULT_NOMEM);
  }
  kp->keycode = keycode;
  kp->modifiers = modifiers;
  kp->type = type;
  return RESULT_SUCCESS(kp);
}

void keypress_debug(keypress_t *kp) {
  if (!kp) {
    return;
  }

  char *modifiers_str = (char *)alloc_page();
  if (!modifiers_str) {
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

  printf("<keypress:%{type: hex} keycode=%{type: hex} modifiers=%{type: str} "
         "type=%{type: "
         "int}>\n",
         PRINT_FLAG_BOTH, (uint64_t)kp, kp->keycode, modifiers_str, kp->type);

  free_page(modifiers_str);
}
