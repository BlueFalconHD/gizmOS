#pragma once

#include <stdint.h>

typedef uint8_t print_flags_t;

#define PRINT_FLAG_TERM 1 << 0
#define PRINT_FLAG_UART 1 << 1
#define PRINT_FLAG_BOTH PRINT_FLAG_TERM | PRINT_FLAG_UART

// https://linuxjedi.co.uk/nested-variadic-functions-in-c/
#include <device/console.h>
#include <device/shared.h>
#include <device/uart.h>
#include <physical_alloc.h>

void print(const char *str, print_flags_t flags);

#define printf(fmt, flags, ...)                                                \
  do {                                                                         \
    char *buf = format(fmt, ##__VA_ARGS__);                                    \
    if (buf) {                                                                 \
      print(buf, flags);                                                       \
      free_page(buf);                                                     \
    }                                                                          \
  } while (0)
