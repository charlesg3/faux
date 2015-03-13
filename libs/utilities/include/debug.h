#pragma once

#include <stdio.h>
#include <utilities/console_colors.h>
#include <sys/types.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

void print_trace (void);

#ifdef __cplusplus
}
#endif

#if 1

#define P() do { fprintf(stderr, "[%d] ./%s:%d:%s\n", getpid(), __FILE__, __LINE__, __FUNCTION__); } while (0)

#define PS(...)                                                         \
    do { char __buffer[4096];                                            \
        snprintf(__buffer,sizeof(__buffer),__VA_ARGS__);                \
        fprintf(stderr, "[%d] %s:%d:%s - %s\n", getpid(), __FILE__, __LINE__, __FUNCTION__, __buffer); \
    } while (0)

#define PB(__str, __len, __data, ...)                                   \
    do { char __buffer[4096]; size_t __i;                               \
        size_t __o = snprintf(__buffer,sizeof(__buffer),(__str), __VA_ARGS__); \
        for (__i = 0; __i < (__len) && __o+2 < sizeof(__buffer); __i++) \
        { __o += sprintf(__buffer + __o, "%02x", ((uint8_t*)(__data))[__i]); } \
        fprintf(stderr, "[%lu] %s:%d:%s - %s\n", getpid(), __FILE__, __LINE__, __FUNCTION__, __buffer); \
    } while (0)

#else

#define P() do { ; } while (0)

#define PS(...) do { ; } while (0)

#define PB(__str, __len, __data, __vaargs...) do { ; } while (0)

#endif



