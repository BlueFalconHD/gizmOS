#include "string_utils.h"
#include "uart.h"

unsigned int strlen(const char *str)
{
    unsigned int len = 0;
    while (*str++) {
        len++;
    }
    return len;
}

int strcmp(const char *s1, const char *s2)
{
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}

void sprintf(char *buffer, const char *format, ...)
{
    va_list args;
    va_start(args, format);

    char *str = buffer;
    const char *fmt = format;

    while (*fmt) {
        if (*fmt != '%') {
            *str++ = *fmt++;
            continue;
        }

        fmt++; /* Skip '%' */

        switch (*fmt) {
            case 'c': {
                char c = (char)va_arg(args, int);
                *str++ = c;
                break;
            }
            case 's': {
                const char *s = va_arg(args, const char *);
                while (*s) {
                    *str++ = *s++;
                }
                break;
            }
            case 'd':
            case 'i': {
                int value = va_arg(args, int);
                if (value < 0) {
                    *str++ = '-';
                    value = -value;
                }
                str = int_to_string(str, value, 10);
                break;
            }
            case 'u': {
                unsigned int value = va_arg(args, unsigned int);
                str = int_to_string(str, value, 10);
                break;
            }
            case 'x': {
                unsigned int value = va_arg(args, unsigned int);
                str = int_to_string(str, value, 16);
                break;
            }
            case 'X': {
                unsigned int value = va_arg(args, unsigned int);
                str = int_to_string_upper(str, value, 16);
                break;
            }
            case '%': {
                *str++ = '%';
                break;
            }
            default: {
                /* Unsupported format specifier */
                *str++ = '%';
                *str++ = *fmt;
                break;
            }
        }
        fmt++;
    }

    *str = '\0'; /* Null-terminate the buffer */
    va_end(args);
}

char *int_to_string(char *str, unsigned int value, int base)
{
    char buffer[32];
    char *ptr = &buffer[31];
    const char *digits = "0123456789abcdef";

    *ptr = '\0';
    do {
        *--ptr = digits[value % base];
        value /= base;
    } while (value != 0);

    /* Copy to str */
    while (*ptr) {
        *str++ = *ptr++;
    }
    return str;
}

char *int_to_string_upper(char *str, unsigned int value, int base)
{
    char buffer[32];
    char *ptr = &buffer[31];
    const char *digits = "0123456789ABCDEF";

    *ptr = '\0';
    do {
        *--ptr = digits[value % base];
        value /= base;
    } while (value != 0);

    /* Copy to str */
    while (*ptr) {
        *str++ = *ptr++;
    }
    return str;
}
