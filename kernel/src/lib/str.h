#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/****************************************
Comparison, Copying, and Concatenation
****************************************/

/**
 * Returns the length of a string.
 *
 * @param str The string.
 */
uint64_t strlen(const char *str);

/**
 * Compares two strings.
 *
 * @param s1 The first string.
 * @param s2 The second string.
 * @return true if the strings are equal, false otherwise.
 */
bool strcmp(const char *s1, const char *s2);

/**
 * Copies a string.
 *
 * @param dest The destination string.
 * @param src The source string.
 */
void strcopy(char *dest, const char *src);

/**
 * Copies n characters of a string.
 *
 * @param dest The destination string.
 * @param src The source string.
 * @param n The number of characters to copy.
 */
void strncopy(char *dest, const char *src, size_t n);

/**
 * Concatenates two strings.
 *
 * @param dest The destination string.
 * @param src The source string.
 */
void strcat(char *dest, const char *src);

/**
 * Concatenates n characters of two strings.
 *
 * @param dest The destination string.
 * @param src The source string.
 * @param n The number of characters to concatenate.
 */
void strncat(char *dest, const char *src, size_t n);

/*********************************
Integer Conversion
*********************************/

/**
 * Converts an unsigned integer to a string in hexadecimal format.
 *
 * @param value The integer.
 * @param buffer The buffer to store the string.
 */
void hexstrfuint(uint64_t value, char *buffer);

/**
 * Converts an unsigned integer to a string.
 *
 * @param value The integer.
 * @param buffer The buffer to store the string.
 */
void strfuint(uint64_t value, char *buffer);

/**
 * Converts a string to an unsigned integer.
 *
 * @param str The string.
 */
uint64_t uintfstr(const char *str);

/**
 * Converts a signed integer to a string in hexadecimal format.
 *
 * @param value The integer.
 * @param buffer The buffer to store the string.
 */
void hexstrfint(int64_t value, char *buffer);

/**
 * Converts a signed integer to a string.
 *
 * @param value The integer.
 * @param buffer The buffer to store the string.
 */
void strfint(int64_t value, char *buffer);

/**
 * Converts a string to a signed integer.
 *
 * @param str The string.
 */
int64_t intfstr(const char *str);
