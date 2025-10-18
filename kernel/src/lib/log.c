#include "log.h"
#include <lib/debug.h>

log_t *g_log_create(const char *subsystem, const char *category) {
  log_t *logger = (log_t *)kalloc(sizeof(log_t));
  if (!logger) {
    dbg("logger == NULL");
    return NULL;
  }

  logger->subsystem = subsystem;
  logger->category = category;
  return logger;
}

void g_log(log_t *logger, log_level_t level, const char *fmt, ...) {}
