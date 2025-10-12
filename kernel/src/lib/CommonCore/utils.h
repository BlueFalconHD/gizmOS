#pragma once

#define PACK __attribute__((packed))
#define ALIGNED(x) __attribute__((aligned(x)))

#define INLINED static inline __attribute__((always_inline))
