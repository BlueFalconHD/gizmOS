#include "string.h"
#include <stdint.h>
#include <stddef.h>

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

// Converts an unsigned integer to a decimal string
void utoa(unsigned int value, char* buffer) {
    char temp[10];
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

// Converts an integer to a decimal string (supports negative numbers)
void itoa(int value, char* buffer) {
    if (value < 0) {
        buffer[0] = '-';
        utoa(-value, buffer + 1);
    } else {
        utoa(value, buffer);
    }
}

// Converts an unsigned integer to a hexadecimal string
void itoa_hex(uintptr_t value, char* buffer) {
    const char* hex_digits = "0123456789ABCDEF";
    char temp[2 * sizeof(uintptr_t)];
    int i = 0;
    if (value == 0) {
        buffer[0] = '0';
        buffer[1] = '\0';
        return;
    }
    while (value > 0) {
        temp[i++] = hex_digits[value % 16];
        value /= 16;
    }
    int j;
    for (j = 0; j < i; j++) {
        buffer[j] = temp[i - j - 1];
    }
    buffer[j] = '\0';
}

void uint64_to_str(uint64_t value, char* buffer) {
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

void uint16_to_str(uint16_t value, char* buffer) {
    char temp[5];
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

void uint8_to_str(uint8_t value, char* buffer) {
    char temp[3];
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

void gz_strcopy(char* dest, const char* src) {
    while (*src) {
        *dest = *src;
        dest++;
        src++;
    }
    *dest = '\0';
}

void gz_strncopy(char* dest, const char* src, size_t n) {
    while (*src && n > 0) {
        *dest = *src;
        dest++;
        src++;
        n--;
    }
    *dest = '\0';
}

void gz_strcat(char* dest, const char* src) {
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

void gz_strncat(char* dest, const char* src, size_t n) {
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
