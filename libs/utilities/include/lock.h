#ifndef FOS_LOCK
#define FOS_LOCK

#include <utilities/decl.h>
#include <utilities/spinlock.h>
#include <stdbool.h>

EXTERN_C_OPEN

typedef spinlock_t FosLock;
#define FosLock_Unlocked SPIN_LOCK_UNLOCKED

void fosLockAcquire(FosLock * in_lock);
int fosLockTry(FosLock * in_lock);
void fosLockRelease(FosLock * in_lock);
void fosLockInit(FosLock * in_lock);

typedef struct FosReaderWriterLock
{
    int num_readers;
    FosLock lock;
} FosReaderWriterLock;

#define FOS_RWLOCK_UNLOCKED { 0, SPIN_LOCK_UNLOCKED }

void fosRWLockInit(FosReaderWriterLock * in_lock);

/* const ptr so it can be used easily with const objects (read-only lock -> const object) */
int fosRWLockTryRead(const FosReaderWriterLock * in_lock);
void fosRWLockAcquireRead(const FosReaderWriterLock * in_lock);
void fosRWLockReleaseRead(const FosReaderWriterLock * in_lock);

int fosRWLockTryWrite(FosReaderWriterLock * in_lock);
void fosRWLockAcquireWrite(FosReaderWriterLock * in_lock);
void fosRWLockReleaseWrite(FosReaderWriterLock * in_lock);

/* Optional interfaces for locks that can be turned into no-ops if threading is disabled. */
#ifndef FOS_SINGLE_THREADED

#define fosLockAcquireThreaded            fosLockAcquire
#define fosLockTryThreaded                fosLockTry
#define fosLockReleaseThreaded            fosLockRelease
#define fosLockInitThreaded               fosLockInit

#define fosRWLockInitThreaded             fosRWLockInit
#define fosRWLockTryReadThreaded          fosRWLockTryRead
#define fosRWLockAcquireReadThreaded      fosRWLockAcquireRead
#define fosRWLockReleaseReadThreaded      fosRWLockReleaseRead
#define fosRWLockTryWriteThreaded         fosRWLockTryWrite
#define fosRWLockAcquireWriteThreaded     fosRWLockAcquireWrite
#define fosRWLockReleaseWriteThreaded     fosRWLockReleaseWrite

#else

#define fosLockAcquireThreaded(x)         ((void)(x))
#define fosLockTryThreaded(x)             (x, true)
#define fosLockReleaseThreaded(x)         ((void)(x))
#define fosLockInitThreaded(x)            ((void)(x))

#define fosRWLockInitThreaded(x)          ((void)(x))
#define fosRWLockTryReadThreaded(x)       (x, true)
#define fosRWLockAcquireReadThreaded(x)   ((void)(x))
#define fosRWLockReleaseReadThreaded(x)   ((void)(x))
#define fosRWLockTryWriteThreaded(x)      (x, true)
#define fosRWLockAcquireWriteThreaded(x)  ((void)(x))
#define fosRWLockReleaseWriteThreaded(x)  ((void)(x))

#endif

EXTERN_C_CLOSE

#endif
