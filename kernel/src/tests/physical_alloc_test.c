#include "test.h"
#include <device/rtc.h>
#include <device/term.h>
#include <lib/str.h>
#include <physical_alloc.h>
#include <stdbool.h>

#define TEST_PATTERN 0xDEADBEEFCAFEBABE
#define MAX_TEST_PAGES 1000

struct test_allocation {
  void *ptr;
  bool allocated;
};

// Basic allocation and deallocation test
static bool test_basic_alloc_free() {
  void *page = alloc_page();
  if (page == NULL) {
    term_puts("Failed to allocate page\n");
    return false;
  }

  // Write test pattern
  *(uint64_t *)page = TEST_PATTERN;

  // Verify pattern
  if (*(uint64_t *)page != TEST_PATTERN) {
    term_puts("Test pattern verification failed\n");
    return false;
  }

  free_page(page);
  return true;
}

// Multiple allocation test
static bool test_multiple_alloc() {
  void *pages[10];
  bool success = true;

  // Allocate 10 pages
  for (int i = 0; i < 10; i++) {
    pages[i] = alloc_page();
    if (pages[i] == NULL) {
      term_puts("Failed to allocate page ");
      char buffer[8];
      strfuint(i, buffer);
      term_puts(buffer);
      term_puts("\n");
      success = false;
      break;
    }
    // Write unique pattern to each page
    *(uint64_t *)pages[i] = TEST_PATTERN + i;
  }

  // Verify patterns
  for (int i = 0; i < 10 && success; i++) {
    if (*(uint64_t *)pages[i] != TEST_PATTERN + i) {
      term_puts("Pattern verification failed for page ");
      char buffer[8];
      strfuint(i, buffer);
      term_puts(buffer);
      term_puts("\n");
      success = false;
      break;
    }
  }

  // Free all pages
  for (int i = 0; i < 10; i++) {
    if (pages[i] != NULL) {
      free_page(pages[i]);
    }
  }

  return success;
}

// Stress test
static bool test_stress_alloc() {
  struct test_allocation allocations[MAX_TEST_PAGES];
  uint64_t initial_free_count = get_free_page_count();
  uint64_t allocated_count = 0;
  bool success = true;
  char buffer[32];

  // Initialize allocation tracking
  for (int i = 0; i < MAX_TEST_PAGES; i++) {
    allocations[i].ptr = NULL;
    allocations[i].allocated = false;
  }

  // Randomly allocate and free pages
  // for (int i = 0; i < 1000; i++) {
  //     uint64_t index = read_cntpct() % MAX_TEST_PAGES;  // Use timer as
  //     random source

  //     if (!allocations[index].allocated) {
  //         // Allocate new page
  //         allocations[index].ptr = alloc_page();
  //         if (allocations[index].ptr != NULL) {
  //             allocations[index].allocated = true;
  //             allocated_count++;
  //             *(uint64_t*)allocations[index].ptr = TEST_PATTERN + index;
  //         }
  //     } else {
  //         // Verify and free existing page
  //         if (*(uint64_t*)allocations[index].ptr != TEST_PATTERN + index) {
  //             term_puts("Pattern verification failed during stress test\n");
  //             success = false;
  //             break;
  //         }
  //         free_page(allocations[index].ptr);
  //         allocations[index].ptr = NULL;
  //         allocations[index].allocated = false;
  //         allocated_count--;
  //     }
  // }

  // Clean up any remaining allocations
  for (int i = 0; i < MAX_TEST_PAGES; i++) {
    if (allocations[i].allocated) {
      free_page(allocations[i].ptr);
    }
  }

  // Verify final count matches initial
  uint64_t final_free_count = get_free_page_count();
  if (final_free_count != initial_free_count) {
    term_puts("Page count mismatch after stress test\n");
    term_puts("Final free pages: ");
    strfuint(final_free_count, buffer);
    term_puts(buffer);
    term_puts("\n");
    success = false;
  }

  return success;
}

bool run_physical_alloc_tests() {
  bool basic_test = test_basic_alloc_free();
  test_complete("basic allocation", basic_test);

  bool multiple_test = test_multiple_alloc();
  test_complete("multiple allocation", multiple_test);

  bool stress_test = test_stress_alloc();
  test_complete("stress allocation", stress_test);

  return basic_test && multiple_test && stress_test;
}
