#include "str.h"
#include <stdint.h>

/****************************************
Comparison, Copying, and Concatenation
****************************************/

uint64_t strlen(const char *str) {
  unsigned int len = 0;
  while (*str++) {
    len++;
  }
  return len;
}

bool strcmp(const char *s1, const char *s2) {
  while (*s1 && *s2) {
    if (*s1 != *s2) {
      return false;
    }
    s1++;
    s2++;
  }
  return *s1 == *s2;
}

void strcopy(char *dest, const char *src) {
  while (*src) {
    *dest = *src;
    dest++;
    src++;
  }
  *dest = '\0';
}

void strncopy(char *dest, const char *src, size_t n) {
  while (*src && n > 0) {
    *dest = *src;
    dest++;
    src++;
    n--;
  }
  *dest = '\0';
}

void strcat(char *dest, const char *src) {
  while (*dest) {
    dest++;
  }
  while (*src) {
    *dest = *src;
    dest++;
    src++;
  }
  *dest = '\0';
}

void strncat(char *dest, const char *src, size_t n) {
  while (*dest) {
    dest++;
  }
  while (*src && n > 0) {
    *dest = *src;
    dest++;
    src++;
    n--;
  }
  *dest = '\0';
}

/*********************************
Integer Conversion
*********************************/

void hexstrfuint(uint64_t value, char *buffer) {
  char temp[20];
  int i = 0;
  if (value == 0) {
    buffer[0] = '0';
    buffer[1] = '\0';
    return;
  }
  while (value > 0) {
    uint8_t digit = value & 0xF;
    temp[i++] = digit < 10 ? '0' + digit : 'A' + digit - 10;
    value >>= 4;
  }
  int j;
  for (j = 0; j < i; j++) {
    buffer[j] = temp[i - j - 1];
  }
  buffer[j] = '\0';
}

void strfuint(uint64_t value, char *buffer) {
  char temp[20];
  int i = 0;
  if (value == 0) {
    buffer[0] = '0';
    buffer[1] = '\0';
    return;
  }
  while (value > 0) {
    temp[i++] = '0' + (value % 10);
    value /= 10;
  }
  int j;
  for (j = 0; j < i; j++) {
    buffer[j] = temp[i - j - 1];
  }
  buffer[j] = '\0';
}

void binstrfuint(uint64_t value, char *buffer) {
  char temp[65];
  int i = 0;
  if (value == 0) {
    buffer[0] = '0';
    buffer[1] = '\0';
    return;
  }
  while (value > 0) {
    temp[i++] = (value & 1) ? '1' : '0';
    value >>= 1;
  }
  int j;
  for (j = 0; j < i; j++) {
    buffer[j] = temp[i - j - 1];
  }
  buffer[j] = '\0';
}

void binstrfint(int64_t value, char *buffer) {
  if (value < 0) {
    *buffer++ = '-';
    value = -value;
  }
  binstrfuint(value, buffer);
}

uint64_t uintfstr(const char *str) {
  uint64_t value = 0;
  while (*str) {
    value = value * 10 + (*str - '0');
    str++;
  }
  return value;
}

void hexstrfint(int64_t value, char *buffer) { hexstrfuint(value, buffer); }

void strfint(int64_t value, char *buffer) {
  if (value < 0) {
    *buffer++ = '-';
    value = -value;
  }
  strfuint(value, buffer);
}

int64_t intfstr(const char *str) {
  int64_t value = 0;
  int sign = 1;
  if (*str == '-') {
    sign = -1;
    str++;
  }
  while (*str) {
    value = value * 10 + (*str - '0');
    str++;
  }
  return sign * value;
}
