#pragma once

#include <sys/fs/fs.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    BCL_ACCESS,
    BCL_DELETE
};

void fosBufferCacheLog(int op, file_id_t id, uint64_t block);
void fosBufferCacheLogReset(void);

#ifdef __cplusplus
}
#endif

