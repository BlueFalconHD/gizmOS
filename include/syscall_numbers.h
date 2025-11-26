// Shared syscall numbers used by kernel and userland.
// Keep these as the single source of truth to avoid drift.
#pragma once

// Process lifecycle
#define SYSNO_EXIT 0x02
#define SYSNO_SPAWN 0x03
#define SYSNO_WAIT 0x04
// Spawn with argv/argc
#define SYSNO_SPAWN2 0x05

// Work / debug
#define SYSNO_PRINT_INT 0x10
#define SYSNO_PRINT_STR 0x11
#define SYSNO_SBRK      0x12

// Notifications
#define SYSNO_NOTIF_REGISTER  0x90
#define SYSNO_NOTIF_UNREGISTER 0x91
#define SYSNO_NOTIF_DONE      0x100

// Spine IPC
#define SYSNO_SPINE_MSG_SEND           0x180
#define SYSNO_SPINE_SERVICE_ADVERTISE  0x181
#define SYSNO_SPINE_SERVICE_LOOKUP     0x182
#define SYSNO_SPINE_GET_SEAL           0x183

// ObjectFS (read-only)
#define SYSNO_OBJ_LOOKUP_PATH     0x220
#define SYSNO_OBJ_STAT            0x221
#define SYSNO_OBJ_LIST_SUBOBJECTS 0x222
#define SYSNO_OBJ_READ            0x223
#define SYSNO_OBJ_ATTR_GET        0x224
#define SYSNO_OBJ_ATTR_LIST       0x225
#define SYSNO_OBJ_DESC            0x226

// ObjectFS (mutation)
#define SYSNO_OBJ_CREATE   0x230
#define SYSNO_OBJ_WRITE    0x231
#define SYSNO_OBJ_SET_ATTR 0x232
#define SYSNO_OBJ_LINK     0x233
#define SYSNO_OBJ_UNLINK   0x234
#define SYSNO_OBJ_RENAME   0x235

// Object handle API
#define SYSNO_OBJH_ID_AT           0x240
#define SYSNO_OBJH_OPEN            0x241
#define SYSNO_OBJH_CLOSE           0x242
#define SYSNO_OBJH_HAS_SUBS        0x243
#define SYSNO_OBJH_SUBS_COUNT      0x244
#define SYSNO_OBJH_SUB_AT          0x245
#define SYSNO_OBJH_OPEN_AT         0x246
#define SYSNO_OBJH_STAT            0x247
#define SYSNO_OBJH_LIST_SUBOBJECTS 0x248
#define SYSNO_OBJH_READ            0x249
#define SYSNO_OBJH_WRITE           0x24A
#define SYSNO_OBJH_ATTR_GET        0x24B
#define SYSNO_OBJH_ATTR_LIST       0x24C
#define SYSNO_OBJH_DESC            0x24D
#define SYSNO_OBJH_CREATE          0x24E
#define SYSNO_OBJH_SET_ATTR        0x24F
#define SYSNO_OBJH_LINK            0x250
#define SYSNO_OBJH_UNLINK          0x251
#define SYSNO_OBJH_RENAME          0x252

// Seek is outside contiguous range by design in kernel header
#define SYSNO_OBJH_SEEK            0x34A

