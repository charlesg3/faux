#ifndef SPACE_ALLOC_H
#define SPACE_ALLOC_H

/**
 * @file space_alloc.h
 * @brief Functions for allocating from a range of numbers.
 *
 * These functions provide the ability to manage a space of numbers. An example of their usage
 * would be for managing file descriptors or process identifiers. The range of numbers is
 * represented by a bit array, so the memory requirement in bytes is the size of the space
 * divided by eight. The size can either be fixed or grow dynamically.
 *
 * @note The largest space supported by these functions is the positive range of an int.
 */

#include <utilities/bitarray.h>

typedef struct SpaceAllocState {
    /**
     * @brief The next unused value.
     *
     * This counter counts up as values are allocated. It indicates the next unused value
     * until it reaches the value of SpaceAllocState::array_size. At that point, unused values
     * must be found through the bit array or the table must be expanded.
     */
    unsigned counter;

    /**
     * @brief The bit array
     *
     * This array has one bit for each allocatable value. A set bit indicates that the
     * corresponding value is allocated.
     */
    long unsigned int *bit_array;

    /** @brief The size of the bit array in units of bits. */
    unsigned array_size;

    /**
     * @brief The number of bits to add each time the table is expanded.
     *
     * This will be zero if the space size is fixed.
     */
    unsigned expansion_size;
} SpaceAllocState;

/**
 * @brief Initialize space allocation state.
 *
 * This must be called before the state can be used with any other functions in this file.
 *
 * @param[in] state
 *      The space allocation state to initialize. This must not be NULL.
 * @param[in] initial_size
 *      The initial size of the space. This size may later be expanded, depending on the value
 *      of \em expansion_size. This value must be non-zero and a multiple of (sizeof(long) * 8).
 * @param[in] expansion_size
 *      The number of space elements to add whenever the available space is entirely allocated.
 *      If this value is 0, the number of allocatable elements will be fixed to \em initial_size
 *      and never expanded. This value must be 0 or a multiple of (sizeof(long) * 8).
 * @param[in] always_lowest
 *      Boolean parameter. If true, the next value to be allocated is always the lowest unused
 *      value. If false, there is no guarentee about which unused value is allocated, but
 *      allocation will often be faster since it is not always necessary to search through the
 *      bit array.
 *
 * @return
 *      0 if the state was initialized successfully or -1 if memory could not be allocated.
 */
int spaceAllocInit(SpaceAllocState *state,
                   unsigned initial_size,
                   unsigned expansion_size,
                   int always_lowest);

/**
 * @brief Destroy space allocation state.
 *
 * Once the state has been destroyed, it may not be used again.
 *
 * @param[in] state
 *      The space allocation state to destroy.
 */
void spaceAllocDestroy(SpaceAllocState *state);

/**
 * @brief Allocate a unit of space.
 *
 * @param[in] state
 *      The space allocation state from which to allocate.
 *
 * @return
 *      The allocated value or -1 if there is no more space to allocate (which may mean that
 *      the expansion size was non-zero but more memory could not be allocated to hold the
 *      additional state).
 */
int spaceAllocAllocate(SpaceAllocState *state);

/**
 * @brief Release an allocated value.
 *
 * This puts the given value back in the pool to be allocated. Note that it is permisible to
 * release a value that has not been allocated as long as it is within the current range (that
 * is, less than the value returned by spaceAllocGetSize().
 *
 * @param[in] state
 *      The space allocation state.
 * @param[in] value
 *      The value to release.
 */
static inline void spaceAllocRelease(SpaceAllocState *state, unsigned value)
{
    fosBitArrayClear(state->bit_array, value);
    if (value == state->counter - 1)
        state->counter = value;
}

/**
 * @brief Get the size of the allocation space.
 */
static inline unsigned spaceAllocGetSize(const SpaceAllocState *state)
{
    return state->array_size;
}

/**
 * @brief Test whether a value is allocated.
 */
static inline int spaceAllocIsAllocated(const SpaceAllocState *state, unsigned value)
{
    return value < state->array_size && fosBitArrayIsSet(state->bit_array, value);
}

#endif
