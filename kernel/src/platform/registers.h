#pragma once

#include <lib/macros.h>
#include <stdint.h>

G_INLINE uint64_t PM_get_hart_id() {
    uint64_t hart_id;
    asm volatile("csrr %0, mhartid" : "=r"(hart_id));
    return hart_id;
}

G_INLINE uint64_t PS_get_status() {
    uint64_t status;
    asm volatile("csrr %0, sstatus" : "=r"(status));
    return status;
}

G_INLINE void PS_set_status(uint64_t status) {
    asm volatile("csrw sstatus, %0" : : "r"(status));
}

G_INLINE uint64_t PS_get_interrupt_enable() {
    uint64_t ie;
    asm volatile("csrr %0, sie" : "=r"(ie));
    return ie;
}

G_INLINE void PS_set_interrupt_enable(uint64_t ie) {
    asm volatile("csrw sie, %0" : : "r"(ie));
}

G_INLINE uint64_t P_get_thread_ptr() {
    uint64_t ptr;
    asm volatile("mv %0, tp" : "=r"(ptr));
    return ptr;
}

G_INLINE void P_set_thread_ptr(uint64_t ptr) {
    asm volatile("mv tp, %0" : : "r"(ptr));
}

G_INLINE uint64_t PS_get_trap_vector() {
    uint64_t vector;
    asm volatile("csrr %0, stvec" : "=r"(vector));
    return vector;
}

G_INLINE void PS_set_trap_vector(uint64_t vector) {
    asm volatile("csrw stvec, %0" : : "r"(vector));
}

G_INLINE uint64_t PS_get_atp() {
    uint64_t ptr;
    asm volatile("csrr %0, satp" : "=r"(ptr));
    return ptr;
}

G_INLINE void PS_set_atp(uint64_t ptr) {
    asm volatile("csrw satp, %0" : : "r"(ptr));
}

G_INLINE uint64_t PS_get_exception_pc() {
    uint64_t pc;
    asm volatile("csrr %0, sepc" : "=r"(pc));
    return pc;
}

G_INLINE void PS_set_exception_pc(uint64_t pc) {
    asm volatile("csrw sepc, %0" : : "r"(pc));
}

G_INLINE uint64_t PS_get_exception_cause() {
    uint64_t cause;
    asm volatile("csrr %0, scause" : "=r"(cause));
    return cause;
}

G_INLINE uint64_t PS_get_exception_value() {
    uint64_t value;
    asm volatile("csrr %0, stval" : "=r"(value));
    return value;
}
