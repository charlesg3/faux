#ifndef _PARAM_H
#define _PARAM_H

#include <config/config.h>

#define SOCK_NUM (1)
#define CORE_NUM (6)
#define INTEL
//#define AMD

#if defined(INTEL)
#define CORE_TO_SOCK(core) ((core) & (SOCK_NUM - 1))
#elif defined(AMD)
#define CORE_TO_SOCK(code) ((core) / (CORE_NUM / SOCK_NUM))
#endif

#define PAGE_SIZE (4096)
#define PAGE_MASK (PAGE_SIZE - 1)
#define CACHELINE_SIZE (64)
#define CACHELINE_MASK (CACHELINE_SIZE - 1)

#endif // _PARAM_H

