#pragma once

#define G_INLINE static inline __attribute__((always_inline))

#ifndef UINT64_MAX
#define UINT64_MAX ((uint64_t)-1)
#endif

#ifndef UINT32_MAX
#define UINT32_MAX ((uint32_t)-1)
#endif

#ifndef UINT16_MAX
#define UINT16_MAX ((uint16_t)-1)
#endif

#ifndef UINT8_MAX
#define UINT8_MAX ((uint8_t)-1)
#endif

#ifndef NULL
#define NULL ((void *)0)
#endif

#ifndef ALIGNUP
#define ALIGNUP(x, a) (((x) + ((a) - 1)) & ~((a) - 1))
#endif

#ifndef ALIGNDOWN
#define ALIGNDOWN(x, a) ((x) & ~((a) - 1))
#endif

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif
