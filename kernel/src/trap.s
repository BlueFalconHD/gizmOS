    .section .bss
    .align 16
    .global trap_stack_bottom
    .global trap_stack_top
trap_stack_bottom:
    .skip 8192
trap_stack_top:

    .section .text
    .global trap_vector
    .align 4

.macro switch_to_trap_stack
    mv t0, sp
    la sp, trap_stack_top
    csrw sscratch, t0
.endm

.macro restore_caller_stack
    csrr sp, sscratch
.endm

.macro save_regs
    addi sp, sp, -256
    sd ra,   0(sp)
    sd t0,   8(sp)
    sd t1,  16(sp)
    sd t2,  24(sp)
    sd s0,  32(sp)
    sd s1,  40(sp)
    sd a0,  48(sp)
    sd a1,  56(sp)
    sd a2,  64(sp)
    sd a3,  72(sp)
    sd a4,  80(sp)
    sd a5,  88(sp)
    sd a6,  96(sp)
    sd a7, 104(sp)
    sd s2, 112(sp)
    sd s3, 120(sp)
    sd s4, 128(sp)
    sd s5, 136(sp)
    sd s6, 144(sp)
    sd s7, 152(sp)
    sd s8, 160(sp)
    sd s9, 168(sp)
    sd s10,176(sp)
    sd s11,184(sp)
    sd t3, 192(sp)
    sd t4, 200(sp)
    sd t5, 208(sp)
    sd t6, 216(sp)
    sd tp, 224(sp)
    sd gp, 232(sp)
.endm

.macro restore_regs
    ld ra,   0(sp)
    ld t0,   8(sp)
    ld t1,  16(sp)
    ld t2,  24(sp)
    ld s0,  32(sp)
    ld s1,  40(sp)
    ld a0,  48(sp)
    ld a1,  56(sp)
    ld a2,  64(sp)
    ld a3,  72(sp)
    ld a4,  80(sp)
    ld a5,  88(sp)
    ld a6,  96(sp)
    ld a7, 104(sp)
    ld s2, 112(sp)
    ld s3, 120(sp)
    ld s4, 128(sp)
    ld s5, 136(sp)
    ld s6, 144(sp)
    ld s7, 152(sp)
    ld s8, 160(sp)
    ld s9, 168(sp)
    ld s10,176(sp)
    ld s11,184(sp)
    ld t3, 192(sp)
    ld t4, 200(sp)
    ld t5, 208(sp)
    ld t6, 216(sp)
    ld tp, 224(sp)
    ld gp, 232(sp)
    addi sp, sp, 256
.endm

trap_vector:
    .cfi_startproc
    .cfi_signal_frame
    switch_to_trap_stack
    .cfi_def_cfa_register t0
    save_regs
    .cfi_def_cfa sp, 256
    .cfi_offset ra, -256
    .cfi_offset s0, -224
    csrr a0, scause
    csrr a1, sepc
    csrr a2, stval
    csrr a3, sstatus
    call kernel_trap_handler
    restore_regs
    .cfi_def_cfa sp, 0
    restore_caller_stack
    .cfi_endproc
    sret

# .section .note.GNU-stack,"",%progbits
