#ifndef BITARRAY_H
#define BITARRAY_H

/**
 * @file bitarray.h
 * @brief Functions for working with arrays of bits.
 *
 * This header provides functions for dealing with arrays of bits. Functions are provided
 * for setting, clearing, and testing arbitrary bits in the array as well as for finding a
 * set or cleared bit in the array.
 *
 * All bit arrays are expected by these functions to contain a multiple of BITARRAY_BITS_PER_INT
 * bits. Use the macro BITARRAY_NINTS() to size an unsigned long array to a desired number
 * of bits.
 *
 * Note that functions in this header do not do any bounds checking. It is up to the caller to
 * ensure that the given bit index is actually in the array.
 */

#include <assert.h>
#include <stddef.h> /// @todo jward - NULL is not defined without stddef.h!?!
#include <system/limits.h>
#include <sys/types.h>

/** @brief The number of bits in a long. */
#define BITARRAY_BITS_PER_INT (8 * sizeof(unsigned int))

/** @brief Create an unsigned long bit mask with only the given bit set. */
#define BITARRAY_CREATE_MASK(bit) (1UL << ((bit) % BITARRAY_BITS_PER_INT))

/** @brief Compute the number of longs needed to hold 'nbits' bits. */
#define BITARRAY_NINTS(nbits) (nbits / BITARRAY_BITS_PER_INT)

/**
 * @brief Test a bit in the array to see if it is set.
 *
 * @param[in] bitarray
 *      The bit array to test.
 * @param[in] bit
 *      The index of the bit to test.
 *
 * @return
 *      0 if the bit is not set or non-zero if it is.
 */
static inline int fosBitArrayIsSet(const unsigned long *bitarray, size_t bit)
{
    assert(bitarray != NULL);
    
    unsigned long mask = BITARRAY_CREATE_MASK(bit);
    return (bitarray[bit / BITARRAY_BITS_PER_INT] & mask) != 0;
}

/**
 * @brief Test a bit in the array to see if it is set.
 *
 * @param[in] bitarray
 *      The bit array to test.
 * @param[in] bit
 *      The index of the bit to test.
 *
 * @return
 *      0 if the bit is not clear or non-zero if it is.
 */
static inline int fosBitArrayIsClear(const unsigned long *bitarray, size_t bit)
{
    assert(bitarray != NULL);
    
    unsigned long mask = BITARRAY_CREATE_MASK(bit);
    return (~bitarray[bit / BITARRAY_BITS_PER_INT] & mask) != 0;
}

/**
 * @brief Set a bit in a bit array.
 *
 * @param[in] bitarray
 *      The bit array.
 * @param[in] bit
 *      The index of the bit to be set.
 */
static inline void fosBitArraySet(unsigned long *bitarray, size_t bit)
{
    assert(bitarray != NULL);
    
    unsigned long mask = BITARRAY_CREATE_MASK(bit);
    bitarray[bit / BITARRAY_BITS_PER_INT] |= mask;
}

/**
 * @brief Set a bit in a bit array.
 *
 * @param[in] bitarray
 *      The bit array.
 * @param[in] bit
 *      The index of the bit to be set.
 */
static inline void fosBitArrayClear(unsigned long *bitarray, size_t bit)
{
    assert(bitarray != NULL);
    
    unsigned long mask = BITARRAY_CREATE_MASK(bit);
    bitarray[bit / BITARRAY_BITS_PER_INT] &= ~mask;
}

/**
 * @brief Find the first set bit in an unsigned long.
 *
 * @param[in] value
 *      The value in which to find a set bit.
 *
 * @return
 *      The index of the first set bit in the value or BITARRAY_BITS_PER_INT if the value
 *      contains no set bits.
 *
 * @todo jward - There are more efficient ways to do this, like the bsf instruction for x86.
 */
static inline size_t fosBitArrayFindFirstSet(unsigned long value)
{
    unsigned long mask;
    size_t index;
    for (mask = 1, index = 0; mask != 0; mask <<= 1, ++index)
    {
        if (value & mask)
            return index;
    }

    return BITARRAY_BITS_PER_INT;
}

/**
 * @brief Find a set bit in a bit array.
 *
 * @param[in] bitarray
 *      The array in which to find a set bit.
 * @param[in] nbits
 *      The number of bits in the given array.
 *
 * @return
 *      The index of a set bit or @em nbits if the array contains no set bits.
 */
static inline size_t fosBitArrayFindSet(const unsigned long *bitarray, size_t nbits)
{
    assert(bitarray != NULL);
    assert(nbits != 0);
    assert(nbits % BITARRAY_BITS_PER_INT == 0);
    
    size_t array_i;
    for (array_i = 0; array_i < BITARRAY_NINTS(nbits); ++array_i)
    {
        if (bitarray[array_i] != 0)
            return array_i * BITARRAY_BITS_PER_INT + fosBitArrayFindFirstSet(bitarray[array_i]);
    }

    return nbits;
}

/**
 * @brief Find a cleared bit in a bit array.
 *
 * @param[in] bitarray
 *      The array in which to find a cleared bit.
 * @param[in] nbits
 *      The number of bits in the given array.
 *
 * @return
 *      The index of a cleared bit or @em nbits if the array contains no cleared bits.
 */
#include <stdio.h>
static inline size_t fosBitArrayFindClear(const unsigned int *bitarray, size_t nbits)
{
    assert(bitarray != NULL);
    assert(nbits != 0);
    assert(nbits % BITARRAY_BITS_PER_INT == 0);
    
    size_t array_i;
    for (array_i = 0; array_i < BITARRAY_NINTS(nbits); ++array_i)
    {
        if (bitarray[array_i] != UINT_MAX)
        {
            return array_i * BITARRAY_BITS_PER_INT + fosBitArrayFindFirstSet(~bitarray[array_i]);
        }
    }

    return nbits;
}

#endif
