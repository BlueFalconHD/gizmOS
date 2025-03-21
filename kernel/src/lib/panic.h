#pragma once

#include <lib/fmt.h>
#include <device/term.h>
#include <hcf.h>

void panic(const char *msg);
void panic_lastresort(const char *msg);
