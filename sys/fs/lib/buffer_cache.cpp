#include <stdio.h>
#include <fcntl.h>
#include <cassert>
#include <cstdint>
#include <memory>
#include <string.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <sys/fs/lib.h>
#include <config/config.h>

static uintptr_t g_bc = 0;

static constexpr auto memory_path = "/faux-shm-42";
static constexpr size_t memory_size = CONFIG_BUFFER_CACHE_SIZE * 1024UL * 1024UL;

void fosLoadBufferCache()
{
    int fd = shm_open(memory_path, O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
    assert(fd >= 0);
    int retval __attribute__((unused)) = ftruncate(fd, memory_size);
    assert(retval == 0);
    void* memory = mmap(nullptr, memory_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    assert(memory != MAP_FAILED);
    close(fd);
    g_bc = (uintptr_t) memory;
}

void fosBufferCacheCopyOut(void *dest, uint64_t offset, uint64_t count)
{
    assert(g_bc);
    assert(offset + count <= memory_size);
    memcpy(dest, (void *)((char *)g_bc + offset), count);
}

void fosBufferCacheCopyIn(uint64_t offset, const void *src, uint64_t count)
{
    assert(g_bc);
    assert(offset + count <= memory_size);
    memcpy((void *)((char *)g_bc + offset), src, count);
}

void fosBufferCacheClear(uint64_t offset, uint64_t count)
{
    assert(g_bc);
    assert(offset + count <= memory_size);
    memset((void *)((char *)g_bc + offset), 0, count);
}
