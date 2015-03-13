#include <stdbool.h>
#include <string.h>
#include <assert.h>

#include <utilities/debug.h>
#include <utilities/tsc.h>

#include <sys/fs/fs.h>
#include <sys/fs/interface.h>
#include <sys/fs/lib.h>
#include <sys/fs/local_ops.h>
#include "dir_cache.hpp"

using namespace std;

using namespace fos;
using namespace fs;
using namespace app;

DirCache *DirCache::g_dc = NULL;

DirCache::~DirCache()
{
}

DirCache::DirCache()
    : m_cache()
{
    assert(g_dc == NULL);
    g_dc = this;
}

int DirCache::resolve(InodeType parent, const char *path, int pathlen, InodeType *child, bool *cached)
{
    if(pathlen == 1 && path[0] == '.')
    {
        *child = parent;
        return OK;
    }

    if(!path || path[0] == '\0')
    {
        *child = parent;
        return OK;
    }

    // first look it up in the cache
#if DIRECTORY_CACHE
    PathPair needle(parent.id, path, pathlen);
    auto dc_i = m_cache.find(needle);
    if(dc_i != m_cache.end())
    {
        if(child) *child = dc_i->second;
        if(cached) *cached = true;
        return OK;
    }
#endif

    parent.server = fosPathOwnern(parent, path, pathlen);

    // next look it up live
    int retval = _fosResolve(parent, path, pathlen, child);

    // store it if it is valid
    if(DIRECTORY_CACHE && retval == OK)
        add(parent, path, pathlen, *child);

    return retval;
}

void DirCache::add(InodeType parent, const char *path, int pathlen, InodeType child)
{
    PathPair needle(parent.id, path, pathlen);
    m_cache.insert(entry_pair(needle, child));
}

void DirCache::remove(InodeType parent, const char *path, int pathlen)
{
    m_cache.erase(PathPair(parent.id, path, pathlen));
}

void DirCache::remove_dest(Inode child)
{
    auto i = m_cache.begin();
    for(; i != m_cache.end(); i++)
        if(i->second.id == child.id && i->second.server == child.server) break;

    if(i != m_cache.end())
        m_cache.erase(i);
}

void DirCache::remove(InodeType parent, const char *path)
{
    return remove(parent, path, strlen(path));
}

void DirCache::clear()
{
    m_cache.clear();
}

