#pragma once
#include <sys/stat.h>
#include <sys/syscall.h>

#ifdef __cplusplus
extern "C" {
#endif

extern void (*vdso_kernel_handler)();

long vdso_invoke(long num, long a0, long a1, long a2, long a3, long a4, long a5);
void vdso_entry();
long vdso_entry_c(long num, long a0, long a1, long a2, long a3, long a4, long a5);

#define vdso_invoke0(sc) vdso_invoke(sc, 0, 0, 0, 0, 0, 0)
#define vdso_invoke1(sc, a) vdso_invoke(sc, (long)a, 0, 0, 0, 0, 0)
#define vdso_invoke2(sc, a, b) vdso_invoke(sc, (long)a, (long)b, 0, 0, 0, 0)
#define vdso_invoke3(sc, a, b, c) vdso_invoke(sc, (long)a, (long)b, (long)c, 0, 0, 0)
#define vdso_invoke4(sc, a, b, c, d) vdso_invoke(sc, (long)a, (long)b, (long)c, (long)d, 0, 0)
#define vdso_invoke5(sc, a, b, c, d, e) vdso_invoke(sc, (long)a, (long)b, (long)c, (long)d, (long)e, 0)
#define vdso_invoke6(sc, a, b, c, d, e, f) vdso_invoke(sc, (long)a, (long)b, (long)c, (long)d, (long)e, (long)f)

#ifdef VDSOWRAP
#define VDSO(...) __VA_ARGS__
#else
#define VDSO(...)
#endif

#include <string.h>
#define hp(s) { real_write(1, s, strlen(s)); }

#ifdef __cplusplus
}
#endif
