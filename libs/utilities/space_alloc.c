
/**
 * @file space_alloc.c
 * @brief
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <system/limits.h>

#include <utilities/bitarray.h>
#include <utilities/space_alloc.h>

#define LINEAR_COUNTER_DISABLED UINT_MAX

static int spaceAllocExpandTable(SpaceAllocState *state)
{
    assert(state != NULL);

    int result = state->array_size;
    unsigned current_bytes = state->array_size / 8;
    unsigned new_size = current_bytes + state->expansion_size;

    // Allocate the new bit array. If this fails, we'll just keep the old array.
    unsigned long *new_bit_array = realloc(state->bit_array, new_size);
    if (new_bit_array == NULL)
        return -1;
    else
        state->bit_array = new_bit_array;

    memset(new_bit_array + current_bytes, 0, state->expansion_size);
    state->array_size = new_size * 8;

    if (state->counter != LINEAR_COUNTER_DISABLED)
        ++state->counter;

    return result;
}

int spaceAllocInit(SpaceAllocState *state,
                   unsigned initial_size,
                   unsigned expansion_size,
                   int always_lowest)
{
    assert(state != NULL);
    assert(initial_size != 0 && initial_size % (sizeof(long) * 8) == 0);
    assert(expansion_size % (sizeof(long) * 8) == 0);

    size_t alloc_size = initial_size / 8;
    state->bit_array = malloc(alloc_size);
    if (state->bit_array == NULL)
        return -1;

    if (always_lowest)
        state->counter = LINEAR_COUNTER_DISABLED;
    else
        state->counter = 0;
    
    state->expansion_size = expansion_size / 8;
    state->array_size = initial_size;
    memset(state->bit_array, 0, alloc_size);
    return 0;
}

void spaceAllocDestroy(SpaceAllocState *state)
{
    free(state->bit_array);
}

int spaceAllocAllocate(SpaceAllocState *state)
{
    assert(state != NULL);

    // First, see if the linear counter is less than the table size.
    if (state->counter < state->array_size)
        return state->counter++;

    // We have reached the end of the array with the linear counter, so see if we can
    // find something that has been released further back in the array.
    int result = (int)fosBitArrayFindClear((const unsigned int *)state->bit_array, state->array_size);
    if (result < state->array_size)
    {
        fosBitArraySet(state->bit_array, result);
        return result;
    }

    // There are no more free values, so expand the array if so configured.
    if (state->expansion_size != 0)
        return spaceAllocExpandTable(state);

    return -1;
}
