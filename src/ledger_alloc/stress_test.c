#include "stress_test.h"
#include "ledger.h"
#include "../uart.h"

void stress_test(int HEAP_SIZE) {

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
