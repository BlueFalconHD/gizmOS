#include "trap_test.h"
#include <device/term.h>
#include <lib/fmt.h>
#include <lib/print.h>

// Function pointer type for system registers
typedef void (*func_ptr)(void);

// Test null pointer dereference
void test_null_pointer_fault(void) {
  print("Testing NULL pointer dereference...\n", PRINT_FLAG_BOTH);
  volatile uint64_t *null_ptr = (volatile uint64_t *)0;
  *null_ptr = 0xDEADBEEF; // Should cause a page fault
}

// Test division by zero
void test_division_by_zero(void) {
  print("Testing division by zero...\n", PRINT_FLAG_BOTH);
  volatile int a = 10;
  volatile int b = 0;
  volatile int c = a / b; // Should cause a division by zero exception

  // This line should never execute
  printf("Result: %{type: int}\n", PRINT_FLAG_BOTH, c);
}

// Test illegal instruction
void test_illegal_instruction(void) {
  print("Testing illegal instruction...\n", PRINT_FLAG_BOTH);

  // Execute an illegal instruction for RISC-V
  // This is a made-up instruction that should be invalid
  asm volatile(".word 0xFFFFFFFF");
}

// Test instruction access fault
void test_instruction_access_fault(void) {
  print("Testing instruction access fault...\n", PRINT_FLAG_BOTH);

  // Try to jump to and execute code from an unmapped address
  func_ptr invalid_func = (func_ptr)0xDEADBEEF;
  invalid_func(); // Should cause an instruction access fault
}

// Define a recursive function locally
void recursive_function(int depth) {
  // Create some local variables to use stack space
  volatile char buffer[256];

  // Fill buffer with some data to ensure it's used
  for (int i = 0; i < 256; i++) {
    buffer[i] = (char)i;
  }

  // Print current depth occasionally
  if (depth % 100 == 0) {
    printf("Stack depth: %{type: int}\n", PRINT_FLAG_BOTH, depth);
  }

  // Recurse without an exit condition
  recursive_function(depth + 1);
}

// Test stack overflow (recursive function with no exit condition)
void test_stack_overflow(void) {
  print("Testing stack overflow...\n", PRINT_FLAG_BOTH);

  // Start the recursion
  recursive_function(0);
}

// Test writing to read-only memory
void test_write_to_readonly(void) {
  print("Testing write to read-only memory...\n", PRINT_FLAG_BOTH);

  // NOTE: This requires you to have some read-only memory mapped
  // For demonstration, we'll use the address of the code section
  // Get address of current function (which should be in read-only text section)
  volatile uint64_t *code_ptr = (volatile uint64_t *)test_write_to_readonly;

  // Try to modify it
  *code_ptr = 0xDEADBEEF; // Should cause a permission fault
}

// Test unaligned memory access (if your architecture checks for this)
void test_unaligned_access(void) {
  print("Testing unaligned memory access...\n", PRINT_FLAG_BOTH);

  // This assumes your architecture generates traps for unaligned access
  // Note that RISC-V may not trap on this depending on configuration
  volatile char buffer[8] = {1, 2, 3, 4, 5, 6, 7, 8};
  volatile uint64_t *unaligned_ptr =
      (volatile uint64_t *)(buffer + 1); // Not 8-byte aligned

  // Read or write unaligned
  *unaligned_ptr = 0xDEADBEEFCAFEBABE; // May cause unaligned access trap

  // This line should never execute if trap occurs
  printf("Unaligned value: 0x%{type: hex}\n", PRINT_FLAG_BOTH, *unaligned_ptr);
}

// Run a specific trap test based on number
void run_trap_test(int test_number) {
  switch (test_number) {
  case 1:
    test_null_pointer_fault();
    break;
  case 2:
    test_division_by_zero();
    break;
  case 3:
    test_illegal_instruction();
    break;
  case 4:
    test_instruction_access_fault();
    break;
  case 5:
    test_stack_overflow();
    break;
  case 6:
    test_write_to_readonly();
    break;
  case 7:
    test_unaligned_access();
    break;
  default:
    printf("Unknown test number: %{type: int}\n", PRINT_FLAG_BOTH, test_number);
  }
}
