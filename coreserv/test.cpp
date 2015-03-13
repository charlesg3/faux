#include <iostream>
#include <messaging/channel.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>
#include <core/core.h>

int
main()
{
    printf("Core is %lu\n", core_get(0));
    printf("Core is %lu\n", core_get(0));
    printf("Core is %lu\n", core_get(0));
    printf("Core is %lu\n", core_get(0));


    printf("Core is %lu\n", core_get(2));
    printf("Core is %lu\n", core_get(2));
    printf("Core is %lu\n", core_get(2));
    printf("Core is %lu\n", core_get(2));
}

