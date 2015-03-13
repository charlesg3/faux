#include <execinfo.h>
#include <stdio.h>
#include <stdlib.h>

/* Obtain a backtrace and print it to stdout. */
void print_trace (void)
{
    void *array[40];
    size_t size;
    char **strings;
    size_t i;

    size = backtrace (array, 40);
    strings = backtrace_symbols (array, size);

    for (i = 0; i < size; i++)
        fprintf(stderr, "%s\n", strings[i]);

    free (strings);
}

