/* src/linker.ld */

/* Set the entry point of the program */
ENTRY(_start)

/* Define memory regions */
SECTIONS
{
    /* Start the code at address 0x40000000 */
    . = 0x40000000;

    /* Text section */
    .text :
    {
        *(.text*)
    }

    /* Read-only data (rodata) */
    .rodata :
    {
        *(.rodata*)
    }

    /* Data section */
    .data :
    {
        __data_start = .;
        *(.data*)
        __data_end = .;
    }

    /* BSS section */
    .bss :
    {
        __bss_start = .;
        *(.bss*)
        __bss_end = .;
    }
}
