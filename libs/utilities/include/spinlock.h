#ifndef __ASM_SPINLOCK_H
#define __ASM_SPINLOCK_H

//#include <mini-os/lib.h>

/*
 * Your basic SMP spinlocks, allowing only a single CPU anywhere
 */

typedef struct {
	volatile unsigned int slock;
} spinlock_t;


#include <utilities/arch/x86_64/arch_spinlock.h>

#define SPINLOCK_MAGIC	0xdead4ead

#define SPIN_LOCK_UNLOCKED ARCH_SPIN_LOCK_UNLOCKED

// #define spin_lock_init(x)	do { *(x) = SPIN_LOCK_UNLOCKED; } while(0)
#define spin_lock_init(x) {spinlock_t __temp_spin_lock = SPIN_LOCK_UNLOCKED; *(x) = __temp_spin_lock;}

/*
 * Simple spin lock operations.  There are two variants, one clears IRQ's
 * on the local processor, one does not.
 *
 * We make no fairness assumptions. They have a cost.
 */

#define spin_is_locked(x)	arch_spin_is_locked(x)

#define spin_unlock_wait(x)	do { barrier(); } while(spin_is_locked(x))


#define _spin_trylock(lock)     ({_raw_spin_trylock(lock) ? \
                                1 : ({ 0;});})

#define _spin_lock(lock)        \
do {                            \
        _raw_spin_lock(lock);   \
} while(0)

#define _spin_unlock(lock)      \
do {                            \
        _raw_spin_unlock(lock); \
} while (0)


#define spin_lock(lock)       _spin_lock(lock)
#define spin_trylock(lock)    _spin_trylock(lock)
#define spin_unlock(lock)       _spin_unlock(lock)

#define DEFINE_SPINLOCK(x) spinlock_t x = SPIN_LOCK_UNLOCKED

#endif
