#pragma once

#include <stdint.h>

void handle_exception_sync();
void handle_exception_irq();
void handle_exception_fiq();
void handle_exception_serr();
