#pragma once

#include "registers.h"
#include <lib/macros.h>

#define SSTATUS_SPP (1L << 8)  // Previous mode, 1=Supervisor, 0=User
#define SSTATUS_SPIE (1L << 5) // Supervisor Previous Interrupt Enable
#define SSTATUS_UPIE (1L << 4) // User Previous Interrupt Enable
#define SSTATUS_SIE (1L << 1)  // Supervisor Interrupt Enable
#define SSTATUS_UIE (1L << 0)  // User Interrupt Enable
#define SSTATUS_SUM   (1L << 18) // Supervisor User Memory Access

#define SIE_EXTERNAL (1 << 9) // External interrupt enable bit in sie register
#define SIE_TIMER (1 << 5)   // Timer interrupt enable bit in sie register
#define SIE_SOFTWARE (1 << 1) // Software interrupt enable bit in sie register
#define SIE_ALL (SIE_EXTERNAL | SIE_TIMER | SIE_SOFTWARE) // All interrupt enable bits



G_INLINE void PS_enable_interrupts(void) {
    uint64_t sstatus = PS_get_status();
    sstatus |= SSTATUS_SIE;
    PS_set_status(sstatus);
}

G_INLINE void PS_disable_interrupts(void) {
    uint64_t sstatus = PS_get_status();
    sstatus &= ~SSTATUS_SIE;
    PS_set_status(sstatus);
}

G_INLINE uint64_t PS_get_interrupt_enabled(void) {
    return PS_get_status() & SSTATUS_SIE;
}

G_INLINE void PS_enable_interrupt_type(uint64_t interrupt) {
    uint64_t sie = PS_get_interrupt_enable();
    sie |= interrupt;
    PS_set_interrupt_enable(sie);
}

G_INLINE void PS_disable_interrupt_type(uint64_t interrupt) {
    uint64_t sie = PS_get_interrupt_enable();
    sie &= ~interrupt;
    PS_set_interrupt_enable(sie);
}

G_INLINE void PS_enable_all_interrupt_types(void) {
    PS_enable_interrupt_type(SIE_ALL);
}

G_INLINE void PS_disable_all_interrupts_types(void) {
    PS_disable_interrupt_type(SIE_ALL);
}
