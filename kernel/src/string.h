#ifndef STRING_H
#define STRING_H

#include <stdint.h>
#include <stddef.h>

#define va_start(ap, param) __builtin_va_start(ap, param)
#define va_arg(ap, type)    __builtin_va_arg(ap, type)
#define va_end(ap)          __builtin_va_end(ap)

unsigned int strlen(const char *str);
int strcmp(const char *s1, const char *s2);

/* Integer to string conversion functions */
void utoa(unsigned int value, char* buffer);
void itoa(int value, char* buffer);
void itoa_hex(uintptr_t value, char* buffer);

void uint64_to_str(uint64_t value, char* buffer);
void uint16_to_str(uint16_t value, char* buffer);
void uint8_to_str(uint8_t value, char* buffer);

void gz_strcpy(char *dest, const char *src);
void gz_strncpy(char *dest, const char *src, size_t n);
void gz_strcat(char *dest, const char *src);
void gz_strncat(char *dest, const char *src, size_t n);

#endif /* STRING_H */
