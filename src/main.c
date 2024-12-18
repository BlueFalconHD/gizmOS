#include "uart.h"
#include "memory.h"
#include "gear.h"
#include "command.h"
#include "string_utils.h"
#include "ledger/ledger.h"

#define HEAP_SIZE 1024 * 1024 /* 1 MB heap size */

uint8_t heap_area[HEAP_SIZE]; /* Heap memory */

// Helper functions for integer to string conversion
void utoa(unsigned int value, char* buffer);
void itoa(int value, char* buffer);
void itoa_hex(uintptr_t value, char* buffer);
void uart_putnbr(unsigned int value);
void uart_putptr(void* ptr);

// void main(void)
// {
//     // uart_puts("Initializing root axel\n");

//     // unsigned int root_axel_gears_capacity = 1024; /* Adjust capacity as needed */
//     // unsigned int root_axel_size = sizeof(axel_t) + (root_axel_gears_capacity * sizeof(gear_t *));

//     // /* Initialize root axel */
//     // axel_t *root = (axel_t *)MEM_START;
//     // root->size = root_axel_gears_capacity;
//     // root->count = 0;

//     // /* Initialize mem_free to point after root axel and gears array */
//     // mem_free = MEM_START + root_axel_size;

//     // uart_puts("Initializing command history\n");

//     // /* Initialize the command history gear */
//     // init_command_history(root);

//     // uart_puts("Finished setup\n");
//     // uart_puts("gizmOS 0.0.1\n");
//     // uart_puts("gzsh repl\n");
//     // repl();


//     ledger_init(heap, HEAP_SIZE);

//     ledger_malloc(16);

// }

void main() {
    // Initialize the allocator with the heap area
    ledger_init(heap_area, HEAP_SIZE);

    uart_puts("Allocator initialized with heap size: ");
    uart_putnbr(HEAP_SIZE);
    uart_puts(" bytes\n");

    // Allocate memory blocks of various sizes
    void* ptr1 = ledger_malloc(100);
    uart_puts("Allocated 100 bytes at ");
    uart_putptr(ptr1);
    uart_puts("\n");

    void* ptr2 = ledger_malloc(200);
    uart_puts("Allocated 200 bytes at ");
    uart_putptr(ptr2);
    uart_puts("\n");

    void* ptr3 = ledger_malloc(50);
    uart_puts("Allocated 50 bytes at ");
    uart_putptr(ptr3);
    uart_puts("\n");

    // Use the allocated memory (e.g., fill with data)
    if (ptr1) {
        uint8_t* data = (uint8_t*)ptr1;
        for (size_t i = 0; i < 100; i++) {
            data[i] = (uint8_t)i;
        }
        uart_puts("Filled 100 bytes of data at ");
        uart_putptr(ptr1);
        uart_puts("\n");
    }

    if (ptr2) {
        uint8_t* data = (uint8_t*)ptr2;
        for (size_t i = 0; i < 200; i++) {
            data[i] = (uint8_t)(i * 2);
        }
        uart_puts("Filled 200 bytes of data at ");
        uart_putptr(ptr2);
        uart_puts("\n");
    }

    // Free some of the allocated memory
    ledger_free(ptr2);
    uart_puts("Freed memory at ");
    uart_putptr(ptr2);
    uart_puts("\n");

    ledger_free(ptr1);
    uart_puts("Freed memory at ");
    uart_putptr(ptr1);
    uart_puts("\n");

    // Allocate a larger block to test coalescing
    void* ptr4 = ledger_malloc(250);
    uart_puts("Allocated 250 bytes at ");
    uart_putptr(ptr4);
    uart_puts("\n");

    // Free remaining memory
    ledger_free(ptr3);
    uart_puts("Freed memory at ");
    uart_putptr(ptr3);
    uart_puts("\n");

    ledger_free(ptr4);
    uart_puts("Freed memory at ");
    uart_putptr(ptr4);
    uart_puts("\n");

    // Stress test: allocate and free many blocks
    const size_t num_blocks = 1000;
    void* pointers[num_blocks];

    size_t i;
    for (i = 0; i < num_blocks; i++) {
        size_t alloc_size = (i % 256) + 16; // Sizes between 16 and 271 bytes
        pointers[i] = ledger_malloc(alloc_size);
        if (!pointers[i]) {
            uart_puts("Allocation failed at iteration ");
            uart_putnbr(i);
            uart_puts("\n");
            break;
        }
    }

    uart_puts("Allocated ");
    uart_putnbr(i);
    uart_puts(" blocks\n");

    // Use the allocated blocks
    for (size_t j = 0; j < i; j++) {
        uint8_t* data = (uint8_t*)pointers[j];
        data[0] = (uint8_t)j; // Simple operation to use the memory
    }

    // Free the allocated blocks in reverse order
    for (size_t j = i; j-- > 0; ) {
        ledger_free(pointers[j]);
    }

    uart_puts("Freed all allocated blocks\n");

    // Final allocation to check if all memory has been coalesced
    void* ptr_final = ledger_malloc(HEAP_SIZE / 2);
    if (ptr_final) {
        uart_puts("Successfully allocated large block after freeing: ");
        uart_putptr(ptr_final);
        uart_puts("\n");
        ledger_free(ptr_final);
    } else {
        uart_puts("Failed to allocate large block after freeing\n");
    }

    uart_puts("Test completed.\n");
}

// Converts an unsigned integer to a decimal string
void utoa(unsigned int value, char* buffer) {
    char temp[10];
    int i = 0;
    if (value == 0) {
        buffer[0] = '0';
        buffer[1] = '\0';
        return;
    }
    while (value > 0) {
        temp[i++] = '0' + (value % 10);
        value /= 10;
    }
    int j;
    for (j = 0; j < i; j++) {
        buffer[j] = temp[i - j - 1];
    }
    buffer[j] = '\0';
}

// Converts an integer to a decimal string (supports negative numbers)
void itoa(int value, char* buffer) {
    if (value < 0) {
        buffer[0] = '-';
        utoa(-value, buffer + 1);
    } else {
        utoa(value, buffer);
    }
}

// Converts an unsigned integer to a hexadecimal string
void itoa_hex(uintptr_t value, char* buffer) {
    const char* hex_digits = "0123456789ABCDEF";
    char temp[2 * sizeof(uintptr_t)];
    int i = 0;
    if (value == 0) {
        buffer[0] = '0';
        buffer[1] = '\0';
        return;
    }
    while (value > 0) {
        temp[i++] = hex_digits[value % 16];
        value /= 16;
    }
    int j;
    for (j = 0; j < i; j++) {
        buffer[j] = temp[i - j - 1];
    }
    buffer[j] = '\0';
}

// Outputs an unsigned integer using uart_puts
void uart_putnbr(unsigned int value) {
    char buffer[11]; // Maximum length for 32-bit unsigned int
    utoa(value, buffer);
    uart_puts(buffer);
}

// Outputs a pointer address in hexadecimal using uart_puts
void uart_putptr(void* ptr) {
    uintptr_t addr = (uintptr_t)ptr;
    uart_puts("0x");
    char buffer[2 * sizeof(uintptr_t) + 1];
    itoa_hex(addr, buffer);
    uart_puts(buffer);
}
