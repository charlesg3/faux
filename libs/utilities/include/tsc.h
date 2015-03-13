#pragma once

static inline unsigned long long rdtscll(void)
{
    unsigned int a,d;
    asm volatile("rdtsc" : "=a" (a), "=d" (d));
    unsigned long long ret = d;
    ret <<= 32;
    ret |= a;
    return ret;
}

#ifdef __cplusplus
extern "C" {
#endif

unsigned long long ts_to_ms(unsigned long long cycles);
unsigned long long ts_to_us(unsigned long long cycles);

#ifdef __cplusplus
}
#endif

