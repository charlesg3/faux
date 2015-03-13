
/**
 * @file fd.c
 * @brief Implementation of the file descriptor library.
 */
#define _GNU_SOURCE

#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <fd/fd.h>
#include <utilities/bitarray.h>
#include <utilities/lock.h>
#include <utilities/space_alloc.h>
#include <system/syscall.h>
#include <config/config.h>

#include <stdint.h>
/** @brief The number of longs added to the bit array every time the table is expanded. */
#define EXPANSION_SIZE 1024

/**
 * @brief The file descriptor table.
 *
 * Generally there will be one of these tables per process. The table needs to be locked in
 * multithreaded environments to protect against concurrent access.
 */
typedef struct FosFDTable
{
    /** @brief A bit array marking once-only FDs. */
    unsigned long once_only_fds;

    /**
     * @brief The FD entry table.
     *
     * This is an array of size FosFDTable::table_size. Every allocated file descriptor has an
     * entry here. Non-allocated file descriptor numbers have a NULL pointer.
     */
    FosFDEntry **fd_entries;

    /** @brief The allocation state. */
    SpaceAllocState alloc_state;

    /** @brief The lock for this structure. */
//    FosReaderWriterLock lock;
} FosFDTable;

/** @brief The global file descriptor table for this process. */
static FosFDTable g_fd_table;

/**
 * @brief Expand the file descriptor table when it is full.
 *
 * The entry table is reallocated to allow for another EXPANSION_SIZE entries. If reallocation
 * fails, the old table remains unchanged.
 *
 * @param[in] table
 *      The file descriptor table to be expanded. The table must be locked by the caller while
 *      this function runs.
 *
 * @return
 *      The next available file descriptor number of the newly expanded table or -ENOMEM if
 *      the needed memory could not be allocated.
 */
static int fdExpandTable(FosFDTable *table)
{
    assert(table != NULL);

    unsigned old_size = spaceAllocGetSize(&table->alloc_state);
    unsigned new_size = old_size + EXPANSION_SIZE;

    // Allocate the new entry pointer array. If this fails, we'll just keep the old array.
    FosFDEntry **new_entry_array = realloc( table->fd_entries, new_size * sizeof(FosFDEntry *) );
    if (new_entry_array == NULL)
        return -ENOMEM;
    else
        table->fd_entries = new_entry_array;

    // Zero out the new portion of the arrays.
    void *new_space = new_entry_array + old_size;
    memset( new_space, 0, EXPANSION_SIZE * sizeof(FosFDEntry *) );
    return 0;
}

/**
 * @brief Find the next unused file descriptor number.
 *
 * Get the next allocation value from table->alloc_state. Some extra work may be done if
 * the table needs to be expanded.
 *
 * @param[in] table
 *      The file descriptor table. The table must be locked by the caller while this function runs.
 *
 * @return
 *      An unused FD number or -ENOMEM if there were no unused FD numbers but memory could not
 *      be allocated to expand the table.
 */
static int fdFindUnusedFD(FosFDTable *table)
{
    assert(table != NULL);

    int pre_alloc_size = (int)spaceAllocGetSize(&table->alloc_state);
    int result = spaceAllocAllocate(&table->alloc_state);

    // If the result is larger than the bit array was before trying to allocate, then
    // the bit array got expanded. That means we need to expand our own FosFDEntry array.
    if (result >= (int)pre_alloc_size)
    {
        int expand_result = fdExpandTable(table);
        if (expand_result < 0)
        {
            spaceAllocRelease(&table->alloc_state, result);
            return expand_result;
        }
    }

    return result;
}

int fdInitialize()
{
    memset( &g_fd_table, 0, sizeof(g_fd_table) );
    //fosRWLockInitThreaded(&g_fd_table.lock);

    int result = spaceAllocInit(&g_fd_table.alloc_state, EXPANSION_SIZE, EXPANSION_SIZE, 1);
    if (result != 0)
        return result;

    // Allocate an initial entry table.
    g_fd_table.fd_entries = malloc( EXPANSION_SIZE * sizeof(FosFDEntry *) );
    if (g_fd_table.fd_entries == NULL)
    {
        spaceAllocDestroy(&g_fd_table.alloc_state);
        return -ENOMEM;
    } else {
        memset(g_fd_table.fd_entries, 0, EXPANSION_SIZE * sizeof(FosFDEntry *));
    }

    return 0;
}

int fdCreate(const FosFileOperations *file_ops, void *private_data, long flags, FosFDEntry **out_entry)
{
    assert(file_ops != NULL);
    assert(out_entry != NULL);

    // Create the FD entry.
    FosFDEntry *entry = malloc( sizeof(FosFDEntry) );
    if (entry == NULL)
        return -ENOMEM;
    memset(entry, 0, sizeof(*entry));

    //fosRWLockAcquireWriteThreaded(&g_fd_table.lock);

    // Find a FD for this file.
    int fd = fdFindUnusedFD(&g_fd_table);
    if (fd < 0)
    {
//        fosRWLockReleaseWriteThreaded(&g_fd_table.lock);
        free(entry);
        return fd;
    } // if

    // Mark this FD as used and add the entry to the entry array.
    g_fd_table.fd_entries[fd] = entry;
//    fosRWLockReleaseWriteThreaded(&g_fd_table.lock);

    entry->fd = fd;
    entry->num_fd = 1;
    entry->file_ops = file_ops;
    entry->private_data = private_data;
    entry->status_flags = flags;
    *out_entry = entry;
    return fd;
}

void fdFlatten(int fd, FosFDEntry *entry, void *private_data)
{
    assert(entry->num_fd > 1);
    entry->num_fd--;

    // Create the FD entry.
    FosFDEntry *new_entry = malloc( sizeof(FosFDEntry) );
    assert(new_entry);
    memset(new_entry, 0, sizeof(*new_entry));

//    fosRWLockAcquireWriteThreaded(&g_fd_table.lock);

    // Mark this FD as used and add the entry to the entry array.
    g_fd_table.fd_entries[fd] = new_entry;
//    fosRWLockReleaseWriteThreaded(&g_fd_table.lock);

    new_entry->fd = fd;
    new_entry->num_fd = 1;
    new_entry->file_ops = entry->file_ops;
    new_entry->private_data = private_data;
    new_entry->status_flags = entry->status_flags;
}

int fdDestroy(int fd)
{
//    fosRWLockAcquireWriteThreaded(&g_fd_table.lock);

    // Make sure that the given fd is sane.
    if ( fd < 0 ||
         fd >= spaceAllocGetSize(&g_fd_table.alloc_state) ||
         !spaceAllocIsAllocated(&g_fd_table.alloc_state, fd) )
    {
//        fosRWLockReleaseWriteThreaded(&g_fd_table.lock);
        return -EBADF;
    }


    assert(g_fd_table.fd_entries[fd] != NULL);
    FosFDEntry *entry = g_fd_table.fd_entries[fd];
    g_fd_table.fd_entries[fd] = NULL;

    // Mark the FD as unused only if it is not once-only.
    if ( fd >= BITARRAY_BITS_PER_INT || fosBitArrayIsClear(&g_fd_table.once_only_fds, fd) )
        spaceAllocRelease(&g_fd_table.alloc_state, fd);
//    fosRWLockReleaseWriteThreaded(&g_fd_table.lock);

    assert(entry != NULL);
    fdFree(entry);

    return 0;
}

FosFDEntry *fdGet(int fd)
{
//    fosRWLockAcquireReadThreaded(&g_fd_table.lock);

    // Make sure that the given fd is sane.
    if ( fd < 0 ||
         fd >= spaceAllocGetSize(&g_fd_table.alloc_state) ||
         g_fd_table.fd_entries[fd] == NULL)
    {
//        fosRWLockReleaseReadThreaded(&g_fd_table.lock);
        return NULL;
    } // if

    FosFDEntry *result = g_fd_table.fd_entries[fd];
//    fosRWLockReleaseReadThreaded(&g_fd_table.lock);

    assert(result->file_ops != NULL);
    return result;
}

void fdCloseAll()
{
    int fd_i;
    int max_fd = (int)spaceAllocGetSize(&g_fd_table.alloc_state);

    for (fd_i = 0; fd_i < max_fd; ++fd_i)
    {
        FosFDEntry *entry = g_fd_table.fd_entries[fd_i];
        if (entry != NULL && entry->file_ops->close != NULL)
            entry->file_ops->close(entry);
    } // for
}

void fdOnceOnly(int fd)
{
    assert(fd >= 0 && fd < BITARRAY_BITS_PER_INT);

//    fosRWLockReleaseWriteThreaded(&g_fd_table.lock);
    fosBitArraySet(&g_fd_table.once_only_fds, fd);
//    fosRWLockReleaseWriteThreaded(&g_fd_table.lock);
}

void fdFree(FosFDEntry *entry) 
{
    if (entry->num_fd <= 1)
        free(entry);
    else
        --(entry->num_fd);
}

int fdDuplicate(FosFDEntry *file, int newfd) 
{
//    fosRWLockAcquireWriteThreaded(&g_fd_table.lock);
    if (newfd < 0) {
        newfd = fdFindUnusedFD(&g_fd_table);
    }
    else if (newfd >= spaceAllocGetSize(&(&g_fd_table)->alloc_state)) {
        fdExpandTable(&g_fd_table);
    }

    FosFDEntry *entry = g_fd_table.fd_entries[newfd];
    if (entry != NULL)
        fdFree(entry);

    ++(file->num_fd);
    g_fd_table.fd_entries[newfd] = file;

//    fosRWLockReleaseWriteThreaded(&g_fd_table.lock);
    return newfd;
}

// FD Map Manipulation:
// These functions are used to keep hare's fds separate
// from the underlying os's fd's
int fd_map[CONFIG_FD_COUNT];

// Allocate a new fd from the map to be used for
// hare
bool fdIsMapped(int fd)
{
    return fd_map[fd] != INT32_MAX;
}

int fdAddInternal(int fd)
{
    int i;
    for(i = 0; i < CONFIG_FD_COUNT; i++)
        if(fd_map[i] == INT32_MAX)
        {
            fd_map[i] = fd + 1;
            return i;
        }

    assert(false);
}

int fdAddExternal(int fd)
{
    int i;
    for(i = 0; i < CONFIG_FD_COUNT; i++)
        if(fd_map[i] == INT32_MAX)
        {
            fd_map[i] = (-1 * fd) - 1;
            return i;
        }

    assert(false);
}

int fdSetInternal(int fd, int internalfd)
{
    fd_map[fd] = internalfd + 1;
}

int fdSetExternal(int fd, int externalfd)
{
    fd_map[fd] = (-1 * externalfd) - 1;
}

int fdRemove(int fd)
{
    fd_map[fd] = INT32_MAX;
}

bool fdIsInternal(int fd)
{
    return fd_map[fd] > 0 && fd_map[fd] != INT32_MAX;
}

bool fdIsExternal(int fd)
{
    return fd_map[fd] < 0 && fd_map[fd] != INT32_MAX;
}


int fdInt(int fd)
{
    assert(fd_map[fd] > 0);
    assert(fd_map[fd] != INT32_MAX);
    return fd_map[fd] - 1;
}

int fdExt(int fd)
{
    if(fd_map[fd] == INT32_MAX) return fd;
    return ((fd_map[fd] + 1) * -1);
}

