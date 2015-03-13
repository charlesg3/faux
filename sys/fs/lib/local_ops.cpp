
#include <iostream>
#include <iomanip>
#include <sstream>
#include <sys/fs/lib.h>
#include <sys/fs/local_ops.h>
#include <utilities/tsc.h>

using namespace std;
using namespace fos;
using namespace fs;

extern int g_process;

uint64_t g_op_count[NUM_CLIENT_EVENTS];
uint64_t g_op_times[NUM_CLIENT_EVENTS];

namespace fos { namespace fs {
void countEvent(int type, uint64_t start)
{
    g_op_count[type]++;
    int64_t elapsed_cycles = rdtscll() - start;
    if(elapsed_cycles > 0)
        g_op_times[type] += elapsed_cycles;
}
}}

LocalOpCounter::LocalOpCounter(int opid)
: m_id(opid)
, m_start(rdtscll())
{
}

LocalOpCounter::~LocalOpCounter()
{
    countEvent(m_id, m_start);
}

void fosShowLocalOps()
{
//    if(g_process != CONFIG_SERVER_COUNT + 1)
//        return;

    std::ostringstream os;
#if ENABLE_DBG_CLIENT_OP_COUNTER
    char * opnames[] = {
        "CREATE   ",
        "DESTROY  ",
        "ADDMAP   ",
        "RM_MAP   ",
        "OPEN     ",
        "READ     ",
        "WRITE    ",
        "LSEEK    ",
        "CLOSE    ",
        "DUP      ",
        "FSTAT    ",
        "DIRENT   ",
        "UNLINK   ",
        "READLINK ",
        "RMDIR    ",
        "TRUNC    ",
        "CHMOD    ",
        "CHOWN    ",
        "RESOLVE  ",
        "UTIMENS  ",
        "SYMLINK  ",
        "BLOCKS   ",
        "FLUSH    ",
        "DIRCACHE ",
        "LOOKUP   ",
        "RPC      ",
        "HASHING  ",
        "OTHER    "};

    os << "------------------------------------------------------------" << endl;
    os << "g_process: " << g_process << endl;
    os << "Local Ops:" << endl;
    os << "Name     " << setw(10) << "Count" << setw(16) << "Time (us)" << setw(16) << "Avg (cycle/op)" <<endl;

    uint64_t count_total = 0;
    uint64_t time_total = 0;
    uint64_t cycles_total = 0;

    for(auto i = 0; i < NUM_FS_TYPES; i++)
    {
        auto count = g_op_count[i];
        auto cycles = g_op_times[i];
        auto time = ts_to_us(cycles);
        auto avg = count > 0 ? cycles / count : 0;
        if(count)
            os << opnames[i] << setw(10) << count << setw(16) << time << setw(16) << avg <<endl;
        count_total += count;
        time_total += time;
        cycles_total += cycles;
    }

    os << "------------------------------------------------------------" << endl;
    os << "TOTAL    " << setw(10) << count_total << setw(16) << time_total << setw(16) << cycles_total / count_total <<endl;
    os << endl;
    os << "------------------------------------------------------------" << endl;
    os << "Other Events:" << endl;

    for(int i = NUM_FS_TYPES; i < NUM_CLIENT_EVENTS; i++)
    {
        auto count = g_op_count[i];
        auto cycles = g_op_times[i];
        auto time = ts_to_us(cycles);
        auto avg = count > 0 ? cycles / count : 0;
        if(count)
            os << opnames[i] << setw(10) << count << setw(16) << time << setw(16) << avg <<endl;
    }
    os << "------------------------------------------------------------" << endl;

#endif

    cout << os.str();

}
