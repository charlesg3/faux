#pragma once
#include <sys/types.h>

#include <sys/fs/fs.h>
#include <sys/fs/interface.h>

enum ClientEvents
{
    DIR_CACHE = NUM_FS_TYPES,
    LOOKUP,
    RPC,
    HASHING,
    OTHER,
    NUM_CLIENT_EVENTS
};

#if ENABLE_DBG_CLIENT_OP_COUNTER
#define OP_COUNTER(op_name) LocalOpCounter _c(op_name);
#define EVENT_START(op_name) uint64_t op_name##_start = rdtscll();
#define EVENT_END(op_name) countEvent(op_name, op_name##_start);
#else
#define OP_COUNTER(op_name)
#define EVENT_START(op_name)
#define EVENT_END(op_name)
#endif

namespace fos { namespace fs {
    class LocalOpCounter
    {
    public:
        LocalOpCounter(int id);
        ~LocalOpCounter();

        int id() const { return m_id; }
        uint64_t start() const { return m_start; }
    private:
        int m_id;
        uint64_t m_start;
    };

    void countEvent(int type, uint64_t time);
}}
