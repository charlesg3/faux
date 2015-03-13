#ifndef _FAUXCORE_H
#define _FAUXCORE_H

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CORE_SERVER_ADDR 1

uint64_t core_get(uint64_t group);

#ifdef __GNUC__
__attribute__ ((unused))
#endif
static int bind_to_core(uint64_t core)
{
    size_t size = CPU_ALLOC_SIZE(40);
    cpu_set_t *mask = CPU_ALLOC(40);
    CPU_ZERO_S(size, mask);
    CPU_SET_S(core, size, mask);
    return pthread_setaffinity_np(pthread_self(), size, mask);
}

int bind_to_default_core();


#ifdef __cplusplus
}
#endif

#endif // _FAUXCORE_H

