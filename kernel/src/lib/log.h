#pragma once

/**
 * logging system for gizmOS kernel
 */

#include <lib/print.h>
#include <lib/types.h>

typedef enum {
  LOG_LEVEL_DEBUG = 0,
  LOG_LEVEL_INFO,
  LOG_LEVEL_WARN,
  LOG_LEVEL_ERROR
} log_level_t;

typedef struct log {
  const char *subsystem;
  const char *category;
} log_t;

log_t *g_log_create(const char *subsystem, const char *category);

void g_log(log_t *logger, log_level_t level, const char *fmt, ...);

#define LOG_DEBUG(logger, fmt, ...)                                            \
  g_log((logger), LOG_LEVEL_DEBUG, fmt, ##__VA_ARGS__)
#define LOG_INFO(logger, fmt, ...)                                             \
  g_log((logger), LOG_LEVEL_INFO, fmt, ##__VA_ARGS__)
#define LOG_WARN(logger, fmt, ...)                                             \
  g_log((logger), LOG_LEVEL_WARN, fmt, ##__VA_ARGS__)
#define LOG_ERROR(logger, fmt, ...)                                            \
  g_log((logger), LOG_LEVEL_ERROR, fmt, ##__VA_ARGS__)
