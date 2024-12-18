#ifndef STRING_UTILS_H
#define STRING_UTILS_H

typedef __builtin_va_list va_list;

#define va_start(ap, param) __builtin_va_start(ap, param)
#define va_arg(ap, type)    __builtin_va_arg(ap, type)
#define va_end(ap)          __builtin_va_end(ap)

unsigned int strlen(const char *str);
int strcmp(const char *s1, const char *s2);
void sprintf(char *buffer, const char *format, ...);

/* Integer to string conversion functions */
char *int_to_string(char *str, unsigned int value, int base);
char *int_to_string_upper(char *str, unsigned int value, int base);

#endif /* STRING_UTILS_H */
