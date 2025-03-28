#pragma once

#include <device/term.h>
#include <hcf.h>
#include <lib/fmt.h>

void panic(const char *msg);
void panic_msg(const char *msg);
void panic_msg_no_cr(const char *msg);
void panic_lastresort(const char *msg);
