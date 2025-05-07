#pragma once

#include "lib/macros.h"
#define KEY_RESERVED 0
#define KEY_ESC 1
#define KEY_1 2
#define KEY_2 3
#define KEY_3 4
#define KEY_4 5
#define KEY_5 6
#define KEY_6 7
#define KEY_7 8
#define KEY_8 9
#define KEY_9 10
#define KEY_0 11
#define KEY_MINUS 12
#define KEY_EQUAL 13
#define KEY_BACKSPACE 14
#define KEY_TAB 15
#define KEY_Q 16
#define KEY_W 17
#define KEY_E 18
#define KEY_R 19
#define KEY_T 20
#define KEY_Y 21
#define KEY_U 22
#define KEY_I 23
#define KEY_O 24
#define KEY_P 25
#define KEY_LEFTBRACE 26
#define KEY_RIGHTBRACE 27
#define KEY_ENTER 28
#define KEY_LEFTCTRL 29
#define KEY_A 30
#define KEY_S 31
#define KEY_D 32
#define KEY_F 33
#define KEY_G 34
#define KEY_H 35
#define KEY_J 36
#define KEY_K 37
#define KEY_L 38
#define KEY_SEMICOLON 39
#define KEY_APOSTROPHE 40
#define KEY_GRAVE 41
#define KEY_LEFTSHIFT 42
#define KEY_BACKSLASH 43
#define KEY_Z 44
#define KEY_X 45
#define KEY_C 46
#define KEY_V 47
#define KEY_B 48
#define KEY_N 49
#define KEY_M 50
#define KEY_COMMA 51
#define KEY_DOT 52
#define KEY_SLASH 53
#define KEY_RIGHTSHIFT 54
#define KEY_KPASTERISK 55
#define KEY_LEFTALT 56
#define KEY_SPACE 57
#define KEY_CAPSLOCK 58
#define KEY_F1 59
#define KEY_F2 60
#define KEY_F3 61
#define KEY_F4 62
#define KEY_F5 63
#define KEY_F6 64
#define KEY_F7 65
#define KEY_F8 66
#define KEY_F9 67
#define KEY_F10 68
#define KEY_NUMLOCK 69
#define KEY_SCROLLLOCK 70
#define KEY_KP7 71
#define KEY_KP8 72
#define KEY_KP9 73
#define KEY_KPMINUS 74
#define KEY_KP4 75
#define KEY_KP5 76
#define KEY_KP6 77
#define KEY_KPPLUS 78
#define KEY_KP1 79
#define KEY_KP2 80
#define KEY_KP3 81
#define KEY_KP0 82
#define KEY_KPDOT 83

#define KEY_ZENKAKUHANKAKU 85
#define KEY_102ND 86
#define KEY_F11 87
#define KEY_F12 88
#define KEY_RO 89
#define KEY_KATAKANA 90
#define KEY_HIRAGANA 91
#define KEY_HENKAN 92
#define KEY_KATAKANAHIRAGANA 93
#define KEY_MUHENKAN 94
#define KEY_KPJPCOMMA 95
#define KEY_KPENTER 96
#define KEY_RIGHTCTRL 97
#define KEY_KPSLASH 98
#define KEY_SYSRQ 99
#define KEY_RIGHTALT 100
#define KEY_LINEFEED 101
#define KEY_HOME 102
#define KEY_UP 103
#define KEY_PAGEUP 104
#define KEY_LEFT 105
#define KEY_RIGHT 106
#define KEY_END 107
#define KEY_DOWN 108
#define KEY_PAGEDOWN 109
#define KEY_INSERT 110
#define KEY_DELETE 111
#define KEY_MACRO 112
#define KEY_MUTE 113
#define KEY_VOLUMEDOWN 114
#define KEY_VOLUMEUP 115
#define KEY_POWER 116 /* SC System Power Down */
#define KEY_KPEQUAL 117
#define KEY_KPPLUSMINUS 118
#define KEY_PAUSE 119
#define KEY_SCALE 120 /* AL Compiz Scale (Expose) */

#define KEY_KPCOMMA 121
#define KEY_HANGEUL 122
#define KEY_HANGUEL KEY_HANGEUL
#define KEY_HANJA 123
#define KEY_YEN 124
#define KEY_LEFTMETA 125
#define KEY_RIGHTMETA 126
#define KEY_COMPOSE 127

#define KEY_STOP 128 /* AC Stop */
#define KEY_AGAIN 129
#define KEY_PROPS 130 /* AC Properties */
#define KEY_UNDO 131  /* AC Undo */
#define KEY_FRONT 132
#define KEY_COPY 133  /* AC Copy */
#define KEY_OPEN 134  /* AC Open */
#define KEY_PASTE 135 /* AC Paste */
#define KEY_FIND 136  /* AC Search */
#define KEY_CUT 137   /* AC Cut */
#define KEY_HELP 138  /* AL Integrated Help Center */
#define KEY_MENU 139  /* Menu (show menu) */
#define KEY_CALC 140  /* AL Calculator */
#define KEY_SETUP 141
#define KEY_SLEEP 142  /* SC System Sleep */
#define KEY_WAKEUP 143 /* System Wake Up */
#define KEY_FILE 144   /* AL Local Machine Browser */
#define KEY_SENDFILE 145
#define KEY_DELETEFILE 146
#define KEY_XFER 147
#define KEY_PROG1 148
#define KEY_PROG2 149
#define KEY_WWW 150 /* AL Internet Browser */
#define KEY_MSDOS 151
#define KEY_COFFEE 152 /* AL Terminal Lock/Screensaver */
#define KEY_SCREENLOCK KEY_COFFEE
#define KEY_ROTATE_DISPLAY 153 /* Display orientation for e.g. tablets */
#define KEY_DIRECTION KEY_ROTATE_DISPLAY
#define KEY_CYCLEWINDOWS 154
#define KEY_MAIL 155
#define KEY_BOOKMARKS 156 /* AC Bookmarks */
#define KEY_COMPUTER 157
#define KEY_BACK 158    /* AC Back */
#define KEY_FORWARD 159 /* AC Forward */
#define KEY_CLOSECD 160
#define KEY_EJECTCD 161
#define KEY_EJECTCLOSECD 162
#define KEY_NEXTSONG 163
#define KEY_PLAYPAUSE 164
#define KEY_PREVIOUSSONG 165
#define KEY_STOPCD 166
#define KEY_RECORD 167
#define KEY_REWIND 168
#define KEY_PHONE 169 /* Media Select Telephone */
#define KEY_ISO 170
#define KEY_CONFIG 171   /* AL Consumer Control Configuration */
#define KEY_HOMEPAGE 172 /* AC Home */
#define KEY_REFRESH 173  /* AC Refresh */
#define KEY_EXIT 174     /* AC Exit */
#define KEY_MOVE 175
#define KEY_EDIT 176
#define KEY_SCROLLUP 177
#define KEY_SCROLLDOWN 178
#define KEY_KPLEFTPAREN 179
#define KEY_KPRIGHTPAREN 180
#define KEY_NEW 181  /* AC New */
#define KEY_REDO 182 /* AC Redo/Repeat */

#define KEY_F13 183
#define KEY_F14 184
#define KEY_F15 185
#define KEY_F16 186
#define KEY_F17 187
#define KEY_F18 188
#define KEY_F19 189
#define KEY_F20 190
#define KEY_F21 191
#define KEY_F22 192
#define KEY_F23 193
#define KEY_F24 194

#define KEY_PLAYCD 200
#define KEY_PAUSECD 201
#define KEY_PROG3 202
#define KEY_PROG4 203
#define KEY_ALL_APPLICATIONS 204 /* AC Desktop Show All Applications */
#define KEY_DASHBOARD KEY_ALL_APPLICATIONS
#define KEY_SUSPEND 205
#define KEY_CLOSE 206 /* AC Close */
#define KEY_PLAY 207
#define KEY_FASTFORWARD 208
#define KEY_BASSBOOST 209
#define KEY_PRINT 210 /* AC Print */
#define KEY_HP 211
#define KEY_CAMERA 212
#define KEY_SOUND 213
#define KEY_QUESTION 214
#define KEY_EMAIL 215
#define KEY_CHAT 216
#define KEY_SEARCH 217
#define KEY_CONNECT 218
#define KEY_FINANCE 219 /* AL Checkbook/Finance */
#define KEY_SPORT 220
#define KEY_SHOP 221
#define KEY_ALTERASE 222
#define KEY_CANCEL 223 /* AC Cancel */
#define KEY_BRIGHTNESSDOWN 224
#define KEY_BRIGHTNESSUP 225
#define KEY_MEDIA 226

#define KEY_SWITCHVIDEOMODE                                                    \
  227 /* Cycle between available video                                         \
         outputs (Monitor/LCD/TV-out/etc) */
#define KEY_KBDILLUMTOGGLE 228
#define KEY_KBDILLUMDOWN 229
#define KEY_KBDILLUMUP 230

#define KEY_SEND 231        /* AC Send */
#define KEY_REPLY 232       /* AC Reply */
#define KEY_FORWARDMAIL 233 /* AC Forward Msg */
#define KEY_SAVE 234        /* AC Save */
#define KEY_DOCUMENTS 235

#define KEY_BATTERY 236

#define KEY_BLUETOOTH 237
#define KEY_WLAN 238
#define KEY_UWB 239

#define KEY_UNKNOWN 240

#define KEY_VIDEO_NEXT 241       /* drive next video source */
#define KEY_VIDEO_PREV 242       /* drive previous video source */
#define KEY_BRIGHTNESS_CYCLE 243 /* brightness up, after max is min */
#define KEY_BRIGHTNESS_AUTO                                                    \
  244 /* Set Auto Brightness: manual                                           \
        brightness control is off,                                             \
        rely on ambient */
#define KEY_BRIGHTNESS_ZERO KEY_BRIGHTNESS_AUTO
#define KEY_DISPLAY_OFF 245 /* display device to off state */

#define KEY_WWAN 246 /* Wireless WAN (LTE, UMTS, GSM, etc.) */
#define KEY_WIMAX KEY_WWAN
#define KEY_RFKILL 247 /* Key that controls all radios */

#define KEY_MICMUTE 248 /* Mute / unmute the microphone */

G_INLINE const char *virtio_keycode_to_string(int keycode) {
  switch (keycode) {
  case KEY_RESERVED:
    return "RESERVED";
  case KEY_ESC:
    return "ESC";
  case KEY_1:
    return "1";
  case KEY_2:
    return "2";
  case KEY_3:
    return "3";
  case KEY_4:
    return "4";
  case KEY_5:
    return "5";
  case KEY_6:
    return "6";
  case KEY_7:
    return "7";
  case KEY_8:
    return "8";
  case KEY_9:
    return "9";
  case KEY_0:
    return "0";
  case KEY_MINUS:
    return "MINUS";
  case KEY_EQUAL:
    return "EQUAL";
  case KEY_BACKSPACE:
    return "BACKSPACE";
  case KEY_TAB:
    return "TAB";
  case KEY_Q:
    return "Q";
  case KEY_W:
    return "W";
  case KEY_E:
    return "E";
  case KEY_R:
    return "R";
  case KEY_T:
    return "T";
  case KEY_Y:
    return "Y";
  case KEY_U:
    return "U";
  case KEY_I:
    return "I";
  case KEY_O:
    return "O";
  case KEY_P:
    return "P";
  case KEY_LEFTBRACE:
    return "LEFTBRACE";
  case KEY_RIGHTBRACE:
    return "RIGHTBRACE";
  case KEY_ENTER:
    return "ENTER";
  case KEY_LEFTCTRL:
    return "LEFTCTRL";
  case KEY_A:
    return "A";
  case KEY_S:
    return "S";
  case KEY_D:
    return "D";
  case KEY_F:
    return "F";
  case KEY_G:
    return "G";
  case KEY_H:
    return "H";
  case KEY_J:
    return "J";
  case KEY_K:
    return "K";
  case KEY_L:
    return "L";
  case KEY_SEMICOLON:
    return "SEMICOLON";
  case KEY_APOSTROPHE:
    return "APOSTROPHE";
  case KEY_GRAVE:
    return "GRAVE";
  case KEY_LEFTSHIFT:
    return "LEFTSHIFT";
  case KEY_BACKSLASH:
    return "BACKSLASH";
  case KEY_Z:
    return "Z";
  case KEY_X:
    return "X";
  case KEY_C:
    return "C";
  case KEY_V:
    return "V";
  case KEY_B:
    return "B";
  case KEY_N:
    return "N";
  case KEY_M:
    return "M";
  case KEY_COMMA:
    return "COMMA";
  case KEY_DOT:
    return "DOT";
  case KEY_SLASH:
    return "SLASH";
  case KEY_RIGHTSHIFT:
    return "RIGHTSHIFT";
  case KEY_KPASTERISK:
    return "KPASTERISK";
  case KEY_LEFTALT:
    return "LEFTALT";
  case KEY_SPACE:
    return "SPACE";
  case KEY_CAPSLOCK:
    return "CAPSLOCK";
  case KEY_F1:
    return "F1";
  case KEY_F2:
    return "F2";
  case KEY_F3:
    return "F3";
  case KEY_F4:
    return "F4";
  case KEY_F5:
    return "F5";
  case KEY_F6:
    return "F6";
  case KEY_F7:
    return "F7";
  case KEY_F8:
    return "F8";
  case KEY_F9:
    return "F9";
  case KEY_F10:
    return "F10";
  case KEY_NUMLOCK:
    return "NUMLOCK";
  case KEY_SCROLLLOCK:
    return "SCROLLLOCK";
  case KEY_KP7:
    return "KP7";
  case KEY_KP8:
    return "KP8";
  case KEY_KP9:
    return "KP9";
  case KEY_KPMINUS:
    return "KPMINUS";
  case KEY_KP4:
    return "KP4";
  case KEY_KP5:
    return "KP5";
  case KEY_KP6:
    return "KP6";
  case KEY_KPPLUS:
    return "KPPLUS";
  case KEY_KP1:
    return "KP1";
  case KEY_KP2:
    return "KP2";
  case KEY_KP3:
    return "KP3";
  case KEY_KP0:
    return "KP0";
  case KEY_KPDOT:
    return "KPDOT";
  case KEY_ZENKAKUHANKAKU:
    return "ZENKAKUHANKAKU";
  case KEY_102ND:
    return "102ND";
  case KEY_F11:
    return "F11";
  case KEY_F12:
    return "F12";
  case KEY_RO:
    return "RO";
  case KEY_KATAKANA:
    return "KATAKANA";
  case KEY_HIRAGANA:
    return "HIRAGANA";
  case KEY_HENKAN:
    return "HENKAN";
  case KEY_KATAKANAHIRAGANA:
    return "KATAKANAHIRAGANA";
  case KEY_MUHENKAN:
    return "MUHENKAN";
  case KEY_KPJPCOMMA:
    return "KPJPCOMMA";
  case KEY_KPENTER:
    return "KPENTER";
  case KEY_RIGHTCTRL:
    return "RIGHTCTRL";
  case KEY_KPSLASH:
    return "KPSLASH";
  case KEY_SYSRQ:
    return "SYSRQ";
  case KEY_RIGHTALT:
    return "RIGHTALT";
  case KEY_LINEFEED:
    return "LINEFEED";
  case KEY_HOME:
    return "HOME";
  case KEY_UP:
    return "UP";
  case KEY_PAGEUP:
    return "PAGEUP";
  case KEY_LEFT:
    return "LEFT";
  case KEY_RIGHT:
    return "RIGHT";
  case KEY_END:
    return "END";
  case KEY_DOWN:
    return "DOWN";
  case KEY_PAGEDOWN:
    return "PAGEDOWN";
  case KEY_INSERT:
    return "INSERT";
  case KEY_DELETE:
    return "DELETE";
  case KEY_MACRO:
    return "MACRO";
  case KEY_MUTE:
    return "MUTE";
  case KEY_VOLUMEDOWN:
    return "VOLUMEDOWN";
  case KEY_VOLUMEUP:
    return "VOLUMEUP";
  case KEY_POWER:
    return "POWER";
  case KEY_KPEQUAL:
    return "KPEQUAL";
  case KEY_KPPLUSMINUS:
    return "KPPLUSMINUS";
  case KEY_PAUSE:
    return "PAUSE";
  case KEY_SCALE:
    return "SCALE";
  case KEY_KPCOMMA:
    return "KPCOMMA";
  case KEY_HANGEUL:
    return "HANGEUL";
  case KEY_HANJA:
    return "HANJA";
  case KEY_YEN:
    return "YEN";
  case KEY_LEFTMETA:
    return "LEFTMETA";
  case KEY_RIGHTMETA:
    return "RIGHTMETA";
  case KEY_COMPOSE:
    return "COMPOSE";
  case KEY_STOP:
    return "STOP";
  case KEY_AGAIN:
    return "AGAIN";
  case KEY_PROPS:
    return "PROPS";
  case KEY_UNDO:
    return "UNDO";
  case KEY_FRONT:
    return "FRONT";
  case KEY_COPY:
    return "COPY";
  case KEY_OPEN:
    return "OPEN";
  case KEY_PASTE:
    return "PASTE";
  case KEY_FIND:
    return "FIND";
  case KEY_CUT:
    return "CUT";
  case KEY_HELP:
    return "HELP";
  case KEY_MENU:
    return "MENU";
  case KEY_CALC:
    return "CALC";
  case KEY_SETUP:
    return "SETUP";
  case KEY_SLEEP:
    return "SLEEP";
  case KEY_WAKEUP:
    return "WAKEUP";
  case KEY_FILE:
    return "FILE";
  case KEY_SENDFILE:
    return "SENDFILE";
  case KEY_DELETEFILE:
    return "DELETEFILE";
  case KEY_XFER:
    return "XFER";
  case KEY_PROG1:
    return "PROG1";
  case KEY_PROG2:
    return "PROG2";
  case KEY_WWW:
    return "WWW";
  case KEY_MSDOS:
    return "MSDOS";
  case KEY_COFFEE:
    return "COFFEE";
  case KEY_ROTATE_DISPLAY:
    return "ROTATE_DISPLAY";
  case KEY_CYCLEWINDOWS:
    return "CYCLEWINDOWS";
  case KEY_MAIL:
    return "MAIL";
  case KEY_BOOKMARKS:
    return "BOOKMARKS";
  case KEY_COMPUTER:
    return "COMPUTER";
  case KEY_BACK:
    return "BACK";
  case KEY_FORWARD:
    return "FORWARD";
  case KEY_CLOSECD:
    return "CLOSECD";
  case KEY_EJECTCD:
    return "EJECTCD";
  case KEY_EJECTCLOSECD:
    return "EJECTCLOSECD";
  case KEY_NEXTSONG:
    return "NEXTSONG";
  case KEY_PLAYPAUSE:
    return "PLAYPAUSE";
  case KEY_PREVIOUSSONG:
    return "PREVIOUSSONG";
  case KEY_STOPCD:
    return "STOPCD";
  case KEY_RECORD:
    return "RECORD";
  case KEY_REWIND:
    return "REWIND";
  case KEY_PHONE:
    return "PHONE";
  case KEY_ISO:
    return "ISO";
  case KEY_CONFIG:
    return "CONFIG";
  case KEY_HOMEPAGE:
    return "HOMEPAGE";
  case KEY_REFRESH:
    return "REFRESH";
  case KEY_EXIT:
    return "EXIT";
  case KEY_MOVE:
    return "MOVE";
  case KEY_EDIT:
    return "EDIT";
  case KEY_SCROLLUP:
    return "SCROLLUP";
  case KEY_SCROLLDOWN:
    return "SCROLLDOWN";
  case KEY_KPLEFTPAREN:
    return "KPLEFTPAREN";
  case KEY_KPRIGHTPAREN:
    return "KPRIGHTPAREN";
  case KEY_NEW:
    return "NEW";
  case KEY_REDO:
    return "REDO";
  case KEY_F13:
    return "F13";
  case KEY_F14:
    return "F14";
  case KEY_F15:
    return "F15";
  case KEY_F16:
    return "F16";
  case KEY_F17:
    return "F17";
  case KEY_F18:
    return "F18";
  case KEY_F19:
    return "F19";
  case KEY_F20:
    return "F20";
  case KEY_F21:
    return "F21";
  case KEY_F22:
    return "F22";
  case KEY_F23:
    return "F23";
  case KEY_F24:
    return "F24";
  case KEY_PLAYCD:
    return "PLAYCD";
  case KEY_PAUSECD:
    return "PAUSECD";
  case KEY_PROG3:
    return "PROG3";
  case KEY_PROG4:
    return "PROG4";
  case KEY_ALL_APPLICATIONS:
    return "ALL_APPLICATIONS";
  case KEY_SUSPEND:
    return "SUSPEND";
  case KEY_CLOSE:
    return "CLOSE";
  case KEY_PLAY:
    return "PLAY";
  case KEY_FASTFORWARD:
    return "FASTFORWARD";
  case KEY_BASSBOOST:
    return "BASSBOOST";
  case KEY_PRINT:
    return "PRINT";
  case KEY_HP:
    return "HP";
  case KEY_CAMERA:
    return "CAMERA";
  case KEY_SOUND:
    return "SOUND";
  case KEY_QUESTION:
    return "QUESTION";
  case KEY_EMAIL:
    return "EMAIL";
  case KEY_CHAT:
    return "CHAT";
  case KEY_SEARCH:
    return "SEARCH";
  case KEY_CONNECT:
    return "CONNECT";
  case KEY_FINANCE:
    return "FINANCE";
  case KEY_SPORT:
    return "SPORT";
  case KEY_SHOP:
    return "SHOP";
  case KEY_ALTERASE:
    return "ALTERASE";
  case KEY_CANCEL:
    return "CANCEL";
  case KEY_BRIGHTNESSDOWN:
    return "BRIGHTNESSDOWN";
  case KEY_BRIGHTNESSUP:
    return "BRIGHTNESSUP";
  case KEY_MEDIA:
    return "MEDIA";
  case KEY_SWITCHVIDEOMODE:
    return "SWITCHVIDEOMODE";
  case KEY_KBDILLUMTOGGLE:
    return "KBDILLUMTOGGLE";
  case KEY_KBDILLUMDOWN:
    return "KBDILLUMDOWN";
  case KEY_KBDILLUMUP:
    return "KBDILLUMUP";
  case KEY_SEND:
    return "SEND";
  case KEY_REPLY:
    return "REPLY";
  case KEY_FORWARDMAIL:
    return "FORWARDMAIL";
  case KEY_SAVE:
    return "SAVE";
  case KEY_DOCUMENTS:
    return "DOCUMENTS";
  case KEY_BATTERY:
    return "BATTERY";
  case KEY_BLUETOOTH:
    return "BLUETOOTH";
  case KEY_WLAN:
    return "WLAN";
  case KEY_UWB:
    return "UWB";
  case KEY_UNKNOWN:
    return "UNKNOWN";
  case KEY_VIDEO_NEXT:
    return "VIDEO_NEXT";
  case KEY_VIDEO_PREV:
    return "VIDEO_PREV";
  case KEY_BRIGHTNESS_CYCLE:
    return "BRIGHTNESS_CYCLE";
  case KEY_BRIGHTNESS_AUTO:
    return "BRIGHTNESS_AUTO";
  case KEY_DISPLAY_OFF:
    return "DISPLAY_OFF";
  case KEY_WWAN:
    return "WWAN";
  case KEY_RFKILL:
    return "RFKILL";
  case KEY_MICMUTE:
    return "MICMUTE";
  default:
    return "UNKNOWN";
  }
}
