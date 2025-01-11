#ifndef STRING_H
#define STRING_H

#include <stdint.h>
typedef __builtin_va_list va_list;

#define va_start(ap, param) __builtin_va_start(ap, param)
#define va_arg(ap, type)    __builtin_va_arg(ap, type)
#define va_end(ap)          __builtin_va_end(ap)

unsigned int strlen(const char *str);
int strcmp(const char *s1, const char *s2);

/* Integer to string conversion functions */
void utoa(unsigned int value, char* buffer);
void itoa(int value, char* buffer);
void itoa_hex(uintptr_t value, char* buffer);

#endif /* STRING_H */
