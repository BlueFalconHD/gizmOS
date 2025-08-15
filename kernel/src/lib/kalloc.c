#include "kalloc.h"
#include "../buddy_allocator.h"
#include "print.h"
#include "str.h"
#include <stddef.h>
#include <stdint.h>

// Allocation tracking array - global as defined in header
allocation_t recent_allocations[100];
static int allocation_index = 0;

// Calculate the order needed for a given size in bytes
static int size_to_order(size_t size) {
    if (size == 0) return 0;
    
    // Each order represents 2^order pages of 4KB each
    // Order 0 = 4KB, Order 1 = 8KB, Order 2 = 16KB, etc.
    size_t page_size = 4096;
    size_t pages_needed = (size + page_size - 1) / page_size; // Round up
    
    if (pages_needed <= 1) return 0;
    
    // Find the order (power of 2) that can fit pages_needed
    int order = 0;
    size_t capacity = 1;
    
    while (capacity < pages_needed && order < 10) { // Max order is 10
        order++;
        capacity <<= 1; // capacity *= 2
    }
    
    return order;
}

// Record an allocation for tracing
static void record_allocation(void *ptr, size_t size, const char *file, int line) {
    allocation_t *alloc = &recent_allocations[allocation_index];
    
    alloc->ptr = ptr;
    alloc->size = size;
    
    // Copy filename (truncate if too long)
    const char *filename = file;
    const char *slash = file;
    
    // Extract just the filename from the path
    while (*slash) {
        if (*slash == '/' || *slash == '\\') {
            filename = slash + 1;
        }
        slash++;
    }
    
    size_t name_len = strlen(filename);
    if (name_len >= sizeof(alloc->file)) {
        name_len = sizeof(alloc->file) - 1;
    }
    
    // Manual copy since we might not have strncpy
    for (size_t i = 0; i < name_len; i++) {
        alloc->file[i] = filename[i];
    }
    alloc->file[name_len] = '\0';
    
    // Convert line number to string
    char line_str[8];
    int len = 0;
    int temp_line = line;
    
    // Handle special case of line 0
    if (line == 0) {
        alloc->line[0] = '0';
        alloc->line[1] = '\0';
    } else {
        // Convert number to string manually
        char temp[8];
        int temp_len = 0;
        
        while (temp_line > 0 && temp_len < 7) {
            temp[temp_len++] = '0' + (temp_line % 10);
            temp_line /= 10;
        }
        
        // Reverse the string
        for (int i = 0; i < temp_len; i++) {
            alloc->line[i] = temp[temp_len - 1 - i];
        }
        alloc->line[temp_len] = '\0';
    }
    
    // Move to next slot, wrapping around
    allocation_index = (allocation_index + 1) % 100;
}

void *kalloc_impl(size_t size) {
    if (size == 0) {
        return NULL;
    }
    
    int order = size_to_order(size);
    void *ptr = buddy_alloc_pages(order);
    
    return ptr;
}

void kfree_impl(void *ptr) {
    if (ptr == NULL) {
        return;
    }
    
    // We need to determine the order that was used for this allocation
    // For now, we'll assume single page allocations (order 0)
    // A more sophisticated implementation would track allocation sizes
    buddy_free_pages(ptr, 0);
}

void *kalloc_trace(size_t size, const char *file, int line) {
    void *ptr = kalloc_impl(size);
    
    if (ptr != NULL) {
        record_allocation(ptr, size, file, line);
        
#ifdef KALLOC_TRACE
        printf("[KALLOC] Allocated %{type: int} bytes at %{type: hex} (%{type: str}:%{type: int})\n",
               PRINT_FLAG_BOTH, (int)size, (uint64_t)ptr, file, line);
#endif
    }
    
    return ptr;
}

void kfree_trace(void *ptr, const char *file, int line) {
    if (ptr != NULL) {
#ifdef KALLOC_TRACE
        printf("[KALLOC] Freeing %{type: hex} (%{type: str}:%{type: int})\n",
               PRINT_FLAG_BOTH, (uint64_t)ptr, file, line);
#endif
    }
    
    kfree_impl(ptr);
}