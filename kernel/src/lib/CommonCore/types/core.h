#pragma once

/**
 * Core Types - Basic type definitions used throughout the kernel.
 *
 * This file aliases and defines an opinionated set of basic types
 * which represent common concepts like sizes, addresses, and boolean values.
 *
 * While it isn't strictly necessary to use these types rather than built-in
 * C types, doing so improves code readability and makes it clear what each
 * value represents.
 */

#include <stdbool.h>
#include <stdint.h>

#define true 1
#define false 0
#define True 1
#define False 0

#define NULL ((void *)0)
#define nil NULL

typedef uint64_t u64; // 64-bit unsigned integer
typedef int64_t i64;  // 64-bit signed integer

typedef uint32_t u32; // 32-bit unsigned integer
typedef int32_t i32;  // 32-bit signed integer

typedef uint16_t u16; // 16-bit unsigned integer
typedef int16_t i16;  // 16-bit signed integer

typedef uint8_t u8; // 8-bit unsigned integer
typedef int8_t i8;  // 8-bit signed integer

typedef uint8_t Bool; // Boolean value (true/false)
typedef u8 Char;      // Single character

#define Str Char *            // Null-terminated unmanaged string
#define ConstStr const Char * // Null-terminated unmanaged constant string

#define Size u64
#define Address u64
#define Ptr void *
