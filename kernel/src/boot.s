        .section .vectors, "ax"
        .align 11
vector_table:
        // Synchronous EL1t
        b handle_sync_el1

        // IRQ EL1t
        b handle_irq_el1

        // FIQ EL1t
        b handle_fiq_el1

        // SError EL1t
        b handle_serr_el1

        // Next 4 are for EL1h
        b handle_sync_el1
        b handle_irq_el1
        b handle_fiq_el1
        b handle_serr_el1

        // Next 4 are for EL0_64
        b handle_sync_el1
        b handle_irq_el1
        b handle_fiq_el1
        b handle_serr_el1


        .equ VTABLE_SIZE, . - vector_table

        .section .text
        .global kmain
kmain:
        # --------------------------------------------------
        # 1. Point VBAR_EL1 to our vector table
        # --------------------------------------------------
        adrp    x0, vector_table
        add     x0, x0, :lo12:vector_table
        msr     VBAR_EL1, x0

        // --------------------------------------------------
        // 2. Jump into C main
        // --------------------------------------------------
        bl      main


handle_sync_el1:
        bl      handle_exception_sync
        eret

handle_irq_el1:
        bl      handle_exception_irq
        eret

handle_fiq_el1:
        bl      handle_exception_fiq
        eret

handle_serr_el1:
        bl      handle_exception_serr
        eret
