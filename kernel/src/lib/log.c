#include "log.h"
#include "lib/ansi.h"
#include <lib/debug.h>

#include <lib/fmt.h>
#include <lib/kalloc.h>
#include <lib/print.h>
#include <stdarg.h>

log_t *g_log_create(const char *subsystem, const char *category) {
  log_t *logger = (log_t *)kalloc(sizeof(log_t));
  if (!logger) {
    dbg("logger == NULL");
    return NULL;
  }

  logger->subsystem = subsystem;
  logger->category = category;
  logger->level = LOG_LEVEL_DEBUG;
  return logger;
}

void g_log(log_t *logger, log_level_t level, const char *fmt, ...) {
  if (!logger) {
    dbg("logger == NULL");
    return;
  }

  if (level < logger->level) {
    return;
  }

  const char *level_color;
  switch (level) {
  case LOG_LEVEL_DEBUG:
    level_color = "" ANSI_COLOR_BLUE;
    break;
  case LOG_LEVEL_INFO:
    level_color = "" ANSI_COLOR_WHITE;
    break;
  case LOG_LEVEL_WARN:
    level_color = "" ANSI_COLOR_YELLOW;
    break;
  case LOG_LEVEL_ERROR:
    level_color = "" ANSI_COLOR_RED;
    break;
  default:
    level_color = "" ANSI_COLOR_WHITE;
    break;
  }

  va_list args;
  va_start(args, fmt);
  char *msg = vformat(fmt, args);
  va_end(args);
  if (!msg) {
    dbg("msg == NULL");
    return;
  }

  char *final_msg =
      format("%{type: str}[%{type: str}:%{type: str}] " ANSI_EFFECT_RESET
             "%{type: str}\n",
             level_color, logger->subsystem, logger->category, msg);

  print(final_msg, PRINT_FLAG_BOTH);
  kfree(msg);
  kfree(final_msg);
}
