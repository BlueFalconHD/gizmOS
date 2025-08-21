#include <lib/macros.h>

G_INLINE void force_exception() {
  // read from null pointer to trigger exception
  volatile int *ptr = (int *)0x0;

  // Dereference the null pointer to cause a fault
  volatile int value = *ptr;
  (void)value; // Prevent unused variable warning
}
