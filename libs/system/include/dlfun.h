#pragma once

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <dlfcn.h>

#define dlfun(func) NULL; *(void **)(&real_##func##_f) = dlsym(RTLD_NEXT, #func)

