#pragma once

#include <cerrno>

#include <messaging/channel.h>
#include <unordered_map>

namespace fos { namespace sched {

    struct exec_shared_t
    {
        volatile int exec_rc;
        volatile int exec_errno;
        volatile int child_started;
        volatile int parent_started;
    };

    struct ChildInfo
    {
        int control_fd;
        volatile struct exec_shared_t *shared;
    };

    typedef std::unordered_map<pid_t, ChildInfo*> ChildMap;

    class SchedServer
    {
    public:
        SchedServer(int id);
        ~SchedServer();

        SchedServer(const SchedServer&) = delete;
        SchedServer& operator=(const SchedServer&) = delete;

        static SchedServer & ss() { return *g_ss; }
        static unsigned int id;
        static void setId(int id_) { SchedServer::id = id_; }
        static bool started() { return g_ss != 0; }

        void exec(int pid, const char *filename, char *args, char *envs, void (*)(void *, int type, int rc, int errno_reply), void *);
        void idle();
        void childExit();

    private:

        int m_id;
        bool m_master;
        ChildMap m_children;

        static SchedServer * g_ss;
    };

}}


