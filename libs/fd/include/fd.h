#pragma once

/**
 * @file fd.h
 * @brief Structures and functions that comprise the file descriptor library.
 *
 * The file descriptor library manages abstract files. Each abstract file is represented by
 * the FosFDEntry struct. A file layer implementation creates one of these entries with
 * fdCreate(). The POSIX file functions then use fdGet() to retrieve the file entry and call
 * the appropriate function in FosFDEntry::file_ops. When a file is closed, the entry must be
 * deallocated with fdDestroy().
 */

#include <sys/types.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

struct FosFDEntry;
struct stat;

/**
 * @brief Function pointers for the various file operations.
 *
 * A file layer implementation must provide a pointer to one of these on all file descriptors
 * associated with it. The functions must adhere to the semantics of the POSIX functions that
 * they implement with two exceptions:
 *
 * -# The functions take a FosFDEntry where the POSIX function would take a file descriptor
 *    (as an int).
 * -# The functions should not set @em errno. Instead, they should return the negated error
 *    code and the caller will set @em errno appropriately.
 *
 * An implementation need not provide every function - unimplemented functions should be set to
 * NULL. All functions must be multithread-safe.
 */
typedef struct FosFileOperations
{
    ssize_t (*read)(struct FosFDEntry *file, void *buf, size_t nbytes);
    ssize_t (*write)(struct FosFDEntry *file, const void *buf, size_t nbytes);
    int (*close)(struct FosFDEntry *file);
    int (*poll)(struct FosFDEntry *file);
    off_t (*lseek)(struct FosFDEntry *file, off_t offset, int whence);
    int (*fstat)(const struct FosFDEntry *file, struct stat *buf);

    /// @todo jward - getdirentries() is not POSIX. Is there a good reason to support it?
    ssize_t (*getdirentries)(const struct FosFDEntry *file, char *buf, size_t nbytes, off_t *basep);

} FosFileOperations;

/**
 * @brief A file descriptor entry.
 *
 * This struct describes an open file. These values are filled in by fdCreate() when the entry
 * is created. The implementation may change any value in this struct except @em fd.
 */
typedef struct FosFDEntry
{
    /** @brief The file descriptor number assigned to this entry. */
    int fd;

    /** @brief The number of file descriptors currently pointing to this entry. */
    int num_fd;

    /** @brief Status flags for this entry. See open(2). */
    long status_flags;

    /** @brief The file operation implementations associated with this entry. */
    const FosFileOperations *file_ops;

    /** @brief Private data used by the implementation. */
    void *private_data;
} FosFDEntry;

/**
 * @brief Initialize file descriptor data structures.
 *
 * This function must be called at process creation time before any file descriptors are created.
 * It is NOT multithread-safe and is expected to be called before any additional threads are
 * spawned.
 *
 * @return
 *      0 if FD initialization succeeded or -ENOMEM if the needed memory could not be allocated.
 */
int fdInitialize();

/**
 * @brief Create a new file descriptor.
 *
 * A new FD entry (FosFDEntry) is allocated and assigned a file descriptor number. This function
 * is multithread-safe.
 *
 * @param[in] file_ops
 *      The file operations struct for this entry. This must not be NULL.
 * @param[in] private_data
 *      Private data used by the implementation. This may be NULL.
 * @param[in] flags
 *      The status flags for this entry. See open(2).
 * @param[out] out_entry
 *      The resulting FD entry is pointed placed here upon successful return. This pointer may
 *      not be NULL. This value is undefined if this function returns an error.
 *
 * @return
 *      The new FD entry's file descriptor number or -ENOMEM if the needed memory could not
 *      be allocated.
 */
int fdCreate(const FosFileOperations *file_ops, void *private_data, long flags, FosFDEntry **out_entry);

/*
 * Similar to create, but is used when a fd has been duped and now we want to
 * have separate instances of the fd (rather than sharing the same internal
 * representation).
 *
 * The consequence is essentially that the file's close() will be called
 * for both fd's rather than just the last one.
 */
void fdFlatten(int fd, FosFDEntry *entry, void *private_data);


/**
 * @brief Destroy a file descriptor after it has been closed.
 *
 * Every file descriptor must eventually be destroyed either with this function or through
 * fdCloseAll(). This function is multithread-safe. The FosFDEntry struct associated with this
 * file descriptor is freed by this function and is no longer valid after it returns.
 *
 * @param[in] fd
 *      The file descriptor number to be destroyed.
 *
 * @return
 *      0 if the file descriptor was successfully destoryed or -EBADF if @em fd was negative or
 *      not an allocated file descriptor.
 */
int fdDestroy(int fd);

/**
 * @brief Get the FD entry for a file descriptor.
 *
 * This function is multithread-safe.
 *
 * @param[in] fd
 *      The file descriptor number.
 *
 * @return
 *      The file descriptor entry or NULL if @em fd is not a valid file descriptor.
 */
FosFDEntry *fdGet(int fd);

/**
 * @brief Close all open files.
 *
 * The FosFileOperations::close function - if it exists - for each file in the file entry table
 * is called. This is intended to be called at process exit so that all open files are cleanly
 * closed - see the warning below. This function is NOT multithread-safe and should only be called
 * by the last thread in the process as it exits.
 *
 * @warning
 *      This function does not free any memory or mark FD numbers as unallocated. FD numbers and
 *      memory will be leaked if the file descriptor library continues to be used after this
 *      function is called.
 */
void fdCloseAll();

/**
 * @brief Specify that a file descriptor should be given out only once.
 *
 * This function allows file descriptors to be marked so that they will never be given out
 * more than once. This is useful for preventing stdout, stdin, and stderr from being reallocated
 * after they are closed.
 *
 * @param[in] fd
 *      The file descriptor to mark as once-only. This must be less than BITARRAY_BITS_PER_LONG.
 *      This fd does not necessarily need to be allocated when this function is called.
 */
void fdOnceOnly(int fd);

int fdDuplicate(FosFDEntry *file, int newfd);

void fdFree(FosFDEntry *entry);


// The following functions are used to keep a mapping
// between the internal file descriptors in faux, versus
// underlying file descriptors in the os.

int fdAddInternal(int fd);
int fdAddExternal(int fd);
int fdSetInternal(int fd, int internalfd);
int fdSetExternal(int fd, int externalfd);
int fdRemove(int fd);
bool fdIsInternal(int fd);
bool fdIsExternal(int fd);
bool fdIsMapped(int fd);
int fdInt(int fd);
int fdExt(int fd);

#ifdef __cplusplus
}
#endif

