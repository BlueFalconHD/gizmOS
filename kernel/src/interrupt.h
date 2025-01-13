#ifndef INTERRUPT_H
#define INTERRUPT_H

#include <stdint.h>

void irq_init_vectors();
void handle_el1_irq();

void kernel_entry();
void kernel_exit();

void handle_irq();

void enable_irq();
void disable_irq();

#endif /* INTERRUPT_H */
