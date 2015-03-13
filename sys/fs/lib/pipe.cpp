#include <errno.h>
#include <stdio.h>
#include <assert.h>
#include <messaging/dispatch.h>
#include <sys/fs/fs.h>
#include <sys/fs/lib.h>

#include "pipe.hpp"

extern "C" {
extern int errno;
extern int g_interrupted;
}

int g_is_reading;

namespace fos { namespace fs { namespace app {

Pipe::Pipe(InodeType inode_, fd_t fd_)
: VFile(inode_, fd_)
, m_should_close(false)
, m_have_closed(false)
{
}

Pipe::~Pipe()
{
}

uint64_t Pipe::read(void *buf, uint64_t count)
{
    g_is_reading = true;
    char *p = (char *)buf;
    uint64_t read_count = 0;

    while(read_count < count)
    {
        // weird issue where we might get a close call in a signal
        // while trying to read from the pipe. That close call needs
        // to turn this read into an EBADF. If we find that we are
        // reading in the close call then we set a flag to close here
        //
        // We have a second flag to ensure that we only try to close
        // once.
        if(m_should_close)
        {
            if(!m_have_closed)
            {
                fosFdClose(m_inode.server, m_fd, -1, UINT64_MAX);
                m_have_closed = true;
            }
            return -EBADF;
        }

        uint64_t read_size = std::min(count - read_count, (uint64_t) FS_CHUNK_SIZE);
        int ret;

again:
        ret = fosFdRead(m_inode.server, m_fd, p + read_count, &read_size);

        if(ret != OK)
        {
            if(ret == -EAGAIN)
            {
                if(g_interrupted)
                {
                    g_is_reading = false;
                    g_interrupted = false;
                    assert(read_count == 0);
                    return -EINTR;
                }
                read_size = std::min(count - read_count, (uint64_t) FS_CHUNK_SIZE);
                goto again;
            }

            g_is_reading = false;
            return read_count ? read_count : ret;
        }
        if(!read_size) break;

        read_count += read_size;
    }

    g_is_reading = false;
    return read_count ? read_count : -EBADF;
}

uint64_t Pipe::write(const void *buf, uint64_t count)
{
    char *p = (char *)buf;
    uint64_t written = 0;

    while(written < count)
    {
        uint64_t write_size = std::min(count - written, (uint64_t) FS_CHUNK_SIZE);
        int ret = fosFdWrite(m_inode.server, m_fd, p + written, &write_size);


        if(ret != OK) return written ? written : ret;
        if(!write_size) break;

        written += write_size;
    }

    return written;
}

int Pipe::close()
{
    m_should_close = true;
    if(g_is_reading)
    {
        return 0;
    }
    else
    {
//        m_have_closed = true;
//        return fosFdClose(m_inode.server, m_fd, -1, UINT64_MAX);
        return 0;
    }
}

}}}
