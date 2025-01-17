#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * Formats a string using the given format string and arguments.
 *
 * @param main The format string.
 * @param ... The arguments to format.
 * @return The formatted string.
 *
 *
 * `%{key: val, ...}`
 *  - type: The type of the value to format. Required. One of "int", "uint", "hex", "char", "str", "ptr".
 *  - case: The case of the output. Optional. One of "lower" (default), "upper".
 *  - justify: The justification of the output. Optional. One of "left", "right" (default).
 *  - sign: The sign of the output. Optional. One of "auto" (default), "force", "space".
 *  - prefix: The prefix of the output. Optional. One of "none" (default), "auto".
 *  - decimal_point: The decimal point of the output. Optional. One of "auto" (default), "force".
 *  - left_pad: The left padding of the output. Optional. One of "space" (default), "zero".
 *  - width: The width of the output. Optional. An integer or "*" (from args).
 *  - precision: The precision of the output. Optional. An integer or "*" (from args).
 */
char *format(const char *main, ...);
