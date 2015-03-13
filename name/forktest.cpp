#include <name/name.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define COUNT 10

int
main()
{
    name_register(0xdeadbeef, endpoint_get_address(endpoint_create()));

    int i;
    pid_t pid[COUNT];
    for(i = 0; i < COUNT; i++)
    {
        pid_t fork_pid = fork();
        if (fork_pid)
            pid[i] = fork_pid;
        else
        {
            Address ep_addr;
            for(int j = 0; j < 10; j++)
                name_lookup(0xdeadbeef, &ep_addr);
            std::cout << "i: " << i << " quitting." << std::endl;
            return 0;
        }
    }

    for(i = 0; i < COUNT; i++)
        waitpid(pid[i], NULL, 0);

    return 0;
}
