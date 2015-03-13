#include <stdbool.h>
#include <utilities/lock.h>
#include <utilities/spinlock.h>

void fosLockAcquire(FosLock * in_lock)
{
    spin_lock(in_lock);
}

int fosLockTry(FosLock * in_lock)
{
    return spin_trylock(in_lock);
}

void fosLockRelease(FosLock * in_lock)
{
    spin_unlock(in_lock);
}

void fosLockInit(FosLock * in_lock)
{
    DEFINE_SPINLOCK(tmp);
    *in_lock = tmp;
}

void fosRWLockInit(FosReaderWriterLock * in_lock)
{
    in_lock->num_readers = 0;
    fosLockInit(&in_lock->lock);
}

int fosRWLockTryRead(const FosReaderWriterLock * in_lock)
{
    FosReaderWriterLock * lock = (FosReaderWriterLock *)in_lock;
    if (!fosLockTry(&lock->lock)) return false;
    ++lock->num_readers;
    fosLockRelease(&lock->lock);
    return true;
}

void fosRWLockAcquireRead(const FosReaderWriterLock * in_lock)
{
    FosReaderWriterLock * lock = (FosReaderWriterLock *)in_lock;
    fosLockAcquire(&lock->lock);
    ++lock->num_readers;
    fosLockRelease(&lock->lock);
}

void fosRWLockReleaseRead(const FosReaderWriterLock * in_lock)
{
    FosReaderWriterLock * lock = (FosReaderWriterLock *)in_lock;
    fosLockAcquire(&lock->lock);
    --lock->num_readers;
    fosLockRelease(&lock->lock);
}

int fosRWLockTryWrite(FosReaderWriterLock * in_lock)
{
    if (!fosLockTry(&in_lock->lock)) return false;

    if (in_lock->num_readers == 0)
        /* Success! -- keep lock */
        return true;

    /* Readers still pending... */
    fosLockRelease(&in_lock->lock);
    return false;
}

void fosRWLockAcquireWrite(FosReaderWriterLock * in_lock)
{
    while (true)
    {
        fosLockAcquire(&in_lock->lock);

        if (in_lock->num_readers == 0)
            /* Success! -- keep lock */
            return;

        /* Readers still pending... */
        fosLockRelease(&in_lock->lock);
    }
}

void fosRWLockReleaseWrite(FosReaderWriterLock * in_lock)
{
    fosLockRelease(&in_lock->lock);
}
