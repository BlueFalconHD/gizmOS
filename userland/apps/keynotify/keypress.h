#pragma once

typedef unsigned keypress_type_t;
typedef keypress_type_t keypress_type;

#define KEYBOARD_KEY_PRESSED ((keypress_type)1)
#define KEYBOARD_KEY_RELEASED ((keypress_type)0)

typedef struct keypress {
  unsigned char keycode;
  unsigned short modifiers;
  keypress_type type;
} __attribute__((packed)) keypress_t;

#define KEYPRESS_MODIFIER_CAPSLOCK 0
#define KEYPRESS_MODIFIER_LSHIFT 1
#define KEYPRESS_MODIFIER_RSHIFT 2
#define KEYPRESS_MODIFIER_LCTRL 3
#define KEYPRESS_MODIFIER_RCTRL 4
#define KEYPRESS_MODIFIER_LALT 5
#define KEYPRESS_MODIFIER_RALT 6
#define KEYPRESS_MODIFIER_LMETA 7
#define KEYPRESS_MODIFIER_RMETA 8

#define KEYPRESS_MODIFIER_SET(mod_var, bit_pos, state)                         \
  do {                                                                         \
    if (state)                                                                 \
      (mod_var) |= (1U << (bit_pos));                                          \
    else                                                                       \
      (mod_var) &= ~(1U << (bit_pos));                                         \
  } while (0)

#define KEYPRESS_MODIFIER_GET(mod, x) ((mod & (1 << x)) ? 1 : 0)

static inline char keypress_rshift(keypress_t *kp) {
  return KEYPRESS_MODIFIER_GET(kp->modifiers, KEYPRESS_MODIFIER_RSHIFT);
}

static inline char keypress_lshift(keypress_t *kp) {
  return KEYPRESS_MODIFIER_GET(kp->modifiers, KEYPRESS_MODIFIER_LSHIFT);
}

static inline char keypress_lctrl(keypress_t *kp) {
  return KEYPRESS_MODIFIER_GET(kp->modifiers, KEYPRESS_MODIFIER_LCTRL);
}

static inline char keypress_rctrl(keypress_t *kp) {
  return KEYPRESS_MODIFIER_GET(kp->modifiers, KEYPRESS_MODIFIER_RCTRL);
}

static inline char keypress_lalt(keypress_t *kp) {
  return KEYPRESS_MODIFIER_GET(kp->modifiers, KEYPRESS_MODIFIER_LALT);
}

static inline char keypress_ralt(keypress_t *kp) {
  return KEYPRESS_MODIFIER_GET(kp->modifiers, KEYPRESS_MODIFIER_RALT);
}

static inline char keypress_lmeta(keypress_t *kp) {
  return KEYPRESS_MODIFIER_GET(kp->modifiers, KEYPRESS_MODIFIER_LMETA);
}

static inline char keypress_rmeta(keypress_t *kp) {
  return KEYPRESS_MODIFIER_GET(kp->modifiers, KEYPRESS_MODIFIER_RMETA);
}

static inline char keypress_capslock(keypress_t *kp) {
  return KEYPRESS_MODIFIER_GET(kp->modifiers, KEYPRESS_MODIFIER_CAPSLOCK);
}
