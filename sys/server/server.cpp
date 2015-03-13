#include <sched.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <iostream>

#include <config/config.h>
#include <messaging/dispatch.h>
#include <messaging/channel.h>
#include <name/name.h>
#include <stdlib.h>

#include "../fs/server.hpp"
#include "../sched/server.hpp"

using namespace std;

using namespace fos;
using namespace fs;
using namespace sched;

void run_server(int id)
{
    enable_pcid();
    FilesystemServer::setId(id);
    SchedServer::setId(id);
    Endpoint *ep = endpoint_create();

    dispatchRegisterResponseEndpoint(ep);
    Address address = endpoint_get_address(ep);
    name_register(1234 + id, address);
    dispatchListen(ep);

    FilesystemServer *fs;
    if(id < CONFIG_FS_SERVER_COUNT)
        fs = new FilesystemServer(ep, id);

    SchedServer *ss = new SchedServer(id);
    dispatchStart();

    if(id < CONFIG_FS_SERVER_COUNT)
        delete fs;
    delete ss;
}

int main(int argc, char **argv)
{
    int fs_servers = 0;
    if(!fs_servers)
        fs_servers = CONFIG_SERVER_COUNT;
    assert(fs_servers);

    fprintf(stderr, "servers: %d\n", fs_servers);

    if(fs_servers > 1)
    {
        pid_t children[fs_servers];

        for(int i = 0; i < fs_servers; i++)
        {
            pid_t child = fork();
            if(child == 0)
            {
                srand(i);
                run_server(i);
                exit(0);
            }
            else
                children[i] = child;
        }

        cout << "filesystem: started." << endl;
        cout.flush();

        for (int i = 0; i < fs_servers; i++)
            waitpid(children[i], NULL, 0);
    }
    else
    {
        cpu_set_t cpu_set;
        CPU_ZERO(&cpu_set);
        CPU_SET(0, &cpu_set);
        sched_setaffinity(0, sizeof(cpu_set), &cpu_set);

//        fprintf(stderr, "server: %d core: %d socket: %d\n", 0, sched_getcpu(), g_socket_map[sched_getcpu()]);

        enable_pcid();
        FilesystemServer::setId(0);
        SchedServer::setId(0);
        Endpoint *ep = endpoint_create();

        dispatchRegisterResponseEndpoint(ep);
        Address address = endpoint_get_address(ep);
        name_register(1234, address);
        dispatchListen(ep);

        FilesystemServer *fs = new FilesystemServer(ep, 0);
        cout << "filesystem: started." << endl;
        cout.flush();
        SchedServer *ss = new SchedServer(0);
        dispatchStart();
        delete fs;
        delete ss;
    }

    return 0;
}

