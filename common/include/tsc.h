#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

static inline uint64_t
rdtsc64(void)
{
    uint32_t low, high;

    __asm__ __volatile__
    (
            "rdtscp"
            : "=a" (low), "=d" (high)
            :
            : "ecx"
    );
    return (((uint64_t) high) << 32 | (uint64_t) low);
}

#ifdef __cplusplus
}
#endif

