#include <utilities/time_arith.h>
#include <config/config.h>
/* Subtract the `struct timeval' values X and Y,
 * storing the result in RESULT.
 * Return 1 if the difference is negative, otherwise 0
 * */
int
timeval_subtract (struct timeval *result, struct timeval *x, struct timeval *y)
{
    /* Perform the carry for the later subtraction by updating y. */
    if (x->tv_usec < y->tv_usec) {
        int nsec = (y->tv_usec - x->tv_usec) / 1000000 + 1;
        y->tv_usec -= 1000000 * nsec;
        y->tv_sec += nsec;
    }
    if (x->tv_usec - y->tv_usec > 1000000) {
        int nsec = (x->tv_usec - y->tv_usec) / 1000000;
        y->tv_usec += 1000000 * nsec;
        y->tv_sec -= nsec;
    }

    /* Compute the time remaining to wait.
     *      tv_usec is certainly positive. */
    if(result)
    {
        result->tv_sec = x->tv_sec - y->tv_sec;
        result->tv_usec = x->tv_usec - y->tv_usec;
    }

    /* Return 1 if result is negative. */
    return (x->tv_sec < y->tv_sec) || (x->tv_sec == y->tv_sec && x->tv_usec < y->tv_usec);
}

void
timeval_add (struct timeval *result, struct timeval *x, struct timeval *y)
{
    struct timeval tmp;
    //assert(x);
    //assert(y);
    tmp.tv_sec = x->tv_sec + y->tv_sec;
    tmp.tv_usec = x->tv_usec + y->tv_usec;

    if(tmp.tv_usec > 1000000)
    {
        tmp.tv_sec += 1;
        tmp.tv_usec -= 1000000;
    }

    *result = tmp;
}

int timeval_diff_ms (struct timeval *x, struct timeval *y)
{
    struct timeval elapsed;
    timeval_subtract(&elapsed, x, y);
    int elapsed_ms = elapsed.tv_sec * 1000 + elapsed.tv_usec / 1000;
    return elapsed_ms;
}

unsigned long long ts_to_ms(unsigned long long cycles)
{
    return cycles / (CONFIG_HARDWARE_KHZ);
}

unsigned long long ts_to_us(unsigned long long cycles)
{
    return cycles / (CONFIG_HARDWARE_KHZ / 1000);
}
