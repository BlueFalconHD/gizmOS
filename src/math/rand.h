#ifndef RAND_H
#define RAND_H

#include <stdint.h>

void init_rand(void);
void srand(uint64_t new_seed);
uint32_t rand(void);


#endif /* RAND_H */
