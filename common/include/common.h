#pragma once

#define warning(...)                                                    \
    do {                                                                \
        char __buf[512];                                                \
        sprintf(__buf, __VA_ARGS__);                                    \
        fprintf(stderr, "%s:%s:%d %s\n", __FUNCTION__, __FILE__, __LINE__, __buf); \
    } while(0)

#define error(...)                                                      \
    do {                                                                \
        char __buf[512];                                                \
        sprintf(__buf, __VA_ARGS__);                                    \
        fprintf(stderr, "%s:%s:%d %s\n", __FUNCTION__, __FILE__, __LINE__, __buf); \
        abort();                                                        \
    } while(0)
