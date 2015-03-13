#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

int gethostname(char *name, size_t len)
{
    size_t hostname_len __attribute__((unused)) = strlen("hare");
    assert(len > hostname_len + 1);
    strcpy(name, "hare");
    return 0;
}
