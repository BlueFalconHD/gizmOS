#pragma once

#include "lib/macros.h"
#include "lib/types.h"
#include <stdint.h>

typedef uint8_t keypress_type_t;
typedef keypress_type_t keypress_type;

#define KEYBOARD_KEY_PRESSED  ((keypress_type)0)
#define KEYBOARD_KEY_RELEASED ((keypress_type)1)

typedef struct keypress {
    uint8_t keycode;
    keypress_type type;
} __attribute__((packed)) keypress_t;

#define KEYPRESS_MODIFIER_CAPSLOCK 0x1
#define KEYPRESS_MODIFIER_LSHIFT 0x2
#define KEYPRESS_MODIFIER_RSHIFT 0x3
#define KEYPRESS_MODIFIER_LCTRL 0x4
#define KEYPRESS_MODIFIER_RCTRL 0x5
#define KEYPRESS_MODIFIER_LALT 0x6
#define KEYPRESS_MODIFIER_RALT 0x7
#define KEYPRESS_MODIFIER_LMETA 0x8
#define KEYPRESS_MODIFIER_RMETA 0x9

#define KEYPRESS_MODIFIER_GET(mod, x) \
    ((mod & (1 << x)) ? 1 : 0)

G_INLINE g_bool keypress_rshift(keypress_t *kp) {
    return KEYPRESS_MODIFIER_GET(kp->keycode, KEYPRESS_MODIFIER_RSHIFT);
}

G_INLINE g_bool keypress_lshift(keypress_t *kp) {
    return KEYPRESS_MODIFIER_GET(kp->keycode, KEYPRESS_MODIFIER_LSHIFT);
}

G_INLINE g_bool keypress_lctrl(keypress_t *kp) {
    return KEYPRESS_MODIFIER_GET(kp->keycode, KEYPRESS_MODIFIER_LCTRL);
}

G_INLINE g_bool keypress_rctrl(keypress_t *kp) {
    return KEYPRESS_MODIFIER_GET(kp->keycode, KEYPRESS_MODIFIER_RCTRL);
}

G_INLINE g_bool keypress_lalt(keypress_t *kp) {
    return KEYPRESS_MODIFIER_GET(kp->keycode, KEYPRESS_MODIFIER_LALT);
}

G_INLINE g_bool keypress_ralt(keypress_t *kp) {
    return KEYPRESS_MODIFIER_GET(kp->keycode, KEYPRESS_MODIFIER_RALT);
}

G_INLINE g_bool keypress_lmeta(keypress_t *kp) {
    return KEYPRESS_MODIFIER_GET(kp->keycode, KEYPRESS_MODIFIER_LMETA);
}

G_INLINE g_bool keypress_rmeta(keypress_t *kp) {
    return KEYPRESS_MODIFIER_GET(kp->keycode, KEYPRESS_MODIFIER_RMETA);
}

G_INLINE g_bool keypress_capslock(keypress_t *kp) {
    return KEYPRESS_MODIFIER_GET(kp->keycode, KEYPRESS_MODIFIER_CAPSLOCK);
}
