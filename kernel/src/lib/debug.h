#pragma once

#include <lib/macros.h>
#include <platform/exception.h>

/*
 * The trap() macro triggers a breakpoint exception in debug builds,
 * allowing a debugger to catch the event. In non-debug builds, it does nothing.
 */

#ifdef G_DEBUG

// non-inline to allow debugger breakpoint setting
void dbg_internal(const char *msg, const char *file, int line);

#define dbg(msg) dbg_internal((msg), __FILE__, __LINE__)

#else

// avoid linkage issues if trap_internal referenced via extern in other files
// shouldn't be an issue but whatever ¯\_(ツ)_/¯
G_INLINE void dbg_internal(const char *msg, const char *file, int line) {
  (void)msg;
  (void)file;
  (void)line;
}

// do nothing
#define dbg(msg) (void)0

#endif
