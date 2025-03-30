#pragma once

#include <device/term.h>
#include <hcf.h>
#include <lib/fmt.h>

void panic(const char *msg);
void panic_msg(const char *msg);
void panic_msg_no_cr(const char *msg);
void panic_location_internal(const char *msg, const char *file, int line);

#define panic_loc(msg) panic_location_internal(msg, __FILE__, __LINE__)
