#ifndef FOS_DECL_H
#define FOS_DECL_H

#ifdef __cplusplus
#define STATIC __attribute__((unused)) inline static
#define INLINE inline
#define EXTERN_C_OPEN extern "C" {
#define EXTERN_C_CLOSE }
#else
#define STATIC __attribute__((unused)) static
#define INLINE
#define EXTERN_C_OPEN
#define EXTERN_C_CLOSE
#endif

#define UNUSED __attribute__((unused))
#define WEAK __attribute__((weak))

#endif // FOS_DECL_H
