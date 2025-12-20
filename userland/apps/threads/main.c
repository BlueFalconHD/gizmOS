#include "../../sys/syscall.h"
#include <stdint.h>

static void puts(const char *s) { sys_print_str(s); }

static uint8_t thread_stack[16 * 1024] __attribute__((aligned(16)));

static uint64_t worker(uint64_t arg) {
  (void)arg;
  puts("worker: hello from thread\n");
  return 42;
}

int main(void) {
  puts("threads: creating thread\n");

  void *stack_top = (void *)(thread_stack + sizeof(thread_stack));
  long tid = sys_thread_create((uint64_t)(uintptr_t)worker, 0, stack_top);
  if (tid < 0) {
    puts("threads: thread.create failed\n");
    return 1;
  }

  int status = -1;
  long r = sys_thread_join_i32(tid, &status);
  if (r < 0) {
    puts("threads: thread.join failed\n");
    return 1;
  }

  if (status == 42) {
    puts("threads: join ok (status==42)\n");
  } else {
    puts("threads: join ok (unexpected status)\n");
  }
  return 0;
}
