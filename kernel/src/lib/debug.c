#include "debug.h"

#ifdef G_DEBUG

// if debugging enabled, don't inline definition so debugger breakpoints work

void dbg_internal(const char *msg, const char *file, int line) {
  (void)msg;
  (void)file;
  (void)line;

#ifdef G_BREAKPOINTS
  // Only trigger a breakpoint if explicitly enabled
  force_breakpoint_exception();
#endif

  return;
}

#endif
