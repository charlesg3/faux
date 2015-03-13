#pragma once
//Simple routines for subracting timevals
//Author: Charles Gruenwald III
#include <sys/time.h>
#include <utilities/tsc.h>

#ifdef __cplusplus
extern "C" {
#endif

int timeval_subtract (struct timeval *result, struct timeval *x, struct timeval *y);
void timeval_add (struct timeval *result, struct timeval *x, struct timeval *y);
int timeval_diff_ms (struct timeval *x, struct timeval *y);

#ifdef __cplusplus
}
#endif

