/* src/boot.s */

/* Symbols provided by the linker */
.extern main          /* The main function in C */
.extern __bss_start   /* Start of BSS */
.extern __bss_end     /* End of BSS */

.section .text
.globl _start

_start:
    /* Set up the stack pointer */
    ldr x0, =__stack_top
    mov sp, x0

    /* Zero initialize the BSS section */
    ldr x1, =__bss_start
    ldr x2, =__bss_end
    mov x3, #0             /* Zero */

zero_loop:
    cmp x1, x2             /* While (x1 < x2) */
    b.ge zero_done
    str x3, [x1], #8       /* Store zero, post-increment address by 8 bytes */
    b zero_loop

zero_done:
    /* Call main */
    bl kmain

halt:
    /* Infinite loop to prevent exit */
    b halt

/* Stack Space */
.section .bss
    .align 12              /* 4096-byte alignment */
    .space 0x4000          /* 16KB stack space */
__stack_top:
