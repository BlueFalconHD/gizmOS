#pragma once

#include <lib/fmt.h>

/*
 * Prints the desired panic message to the console and halts the system.
 */
void panic(const char *msg);

/*
 * Panic-formatted print
 * Prints in a format like: PANIC: <message>
 */
void panic_msg(const char *msg);

/*
 * Display a panic-formatted message without a carriage return for additional
 * information (numbers, strings, etc.) to be printed on the same line.
 */
void panic_msg_no_cr(const char *msg);

/*
 * Panic with a message and the location of the panic.
 * This function is used internally to provide the file and line number
 */
void panic_location_internal(const char *msg, const char *file, int line);

/*
 * Halt the system with an indefinite loop.
 */
void panic_halt();

#define panic_loc(msg) panic_location_internal(msg, __FILE__, __LINE__)
