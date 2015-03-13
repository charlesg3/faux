#ifndef _UTIL_H
#define _UTIL_H

#include <sys/syscall.h>
#include <unistd.h>

#define likely(x) __builtin_expect((x), 1)
#define unlikely(x) __builtin_expect((x), 0)
#define cmpxchg(...) __sync_bool_compare_and_swap(__VA_ARGS__)
#define barrier() __asm__ __volatile__ ("" ::: "memory")
#define cpu_relax() __asm__ __volatile__ ("pause" ::: "memory")
#define mb() __asm__ __volatile__ ("mfence" ::: "memory")
#define rmb() __asm__ __volatile__ ("lfence" ::: "memory")
#define wmb() __asm__ __volatile__ ("sfence" ::: "memory")

#ifdef __cplusplus
extern "C"
#endif
inline int gettid(void)
{
    return syscall(__NR_gettid);
}

#endif // _UTIL_H

