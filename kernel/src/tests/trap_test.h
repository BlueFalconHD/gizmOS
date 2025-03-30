#ifndef TRAP_TEST_H
#define TRAP_TEST_H

// Test various trap conditions
void test_null_pointer_fault(void);
void test_division_by_zero(void);
void test_illegal_instruction(void);
void test_instruction_access_fault(void);
void test_stack_overflow(void);
void test_write_to_readonly(void);
void test_unaligned_access(void);

typedef enum {
  TRAP_TEST_NULL_POINTER_FAULT = 1,
  TRAP_TEST_DIVISION_BY_ZERO,
  TRAP_TEST_ILLEGAL_INSTRUCTION,
  TRAP_TEST_INSTRUCTION_ACCESS_FAULT,
  TRAP_TEST_STACK_OVERFLOW,
  TRAP_TEST_WRITE_TO_READONLY,
  TRAP_TEST_UNALIGNED_ACCESS,
} trap_test_t;

// Run a specific trap test (1-7)
void run_trap_test(int test_number);

#endif /* TRAP_TEST_H */
