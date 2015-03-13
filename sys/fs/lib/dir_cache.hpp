#pragma once
#pragma GCC diagnostic ignored "-fpermissive"

#include <tr1/unordered_map>
#include <map>
#include <string>

#include <sys/fs/lib.h>
#include <sys/fs/fs.h>

namespace fos { namespace fs { namespace app {

    class PathPair
    {
    public:
        PathPair(file_id_t id_, const char *str_, int len_)
        : id(id_), len(len_), str((char *)str_), copy(false)
        {
        }

        PathPair(const PathPair &copyfrom)
        : id(copyfrom.id), len(copyfrom.len), copy(true)
        {
            str = (char *)malloc(len + 1);
            memcpy(str, copyfrom.str, len);
            str[len] = '\0';
        }

        PathPair(PathPair &copyfrom)
        : id(copyfrom.id), len(copyfrom.len), copy(true)
        {
            str = (char *)malloc(len + 1);
            memcpy(str, copyfrom.str, len);
            str[len] = '\0';
        }

        ~PathPair()
        {
            if(copy) free(str);
        }

        friend bool operator== (const PathPair &pp1, const PathPair &pp2)
        {
            return ((pp1.id == pp2.id) && (pp1.len == pp2.len) && strncmp(pp1.str, pp2.str, pp1.len) == 0);
        }

        friend bool operator< (const PathPair &pp1, const PathPair &pp2)
        {
            if(pp1.id != pp2.id) return pp1.id < pp2.id;
            if(pp1.len != pp2.len) return pp1.len < pp2.len;
            return strncmp(pp1.str, pp2.str, pp1.len) < 0;
        }

        friend bool operator> (const PathPair &pp1, const PathPair &pp2)
        {
            if(pp1.id != pp2.id) return pp1.id > pp2.id;
            if(pp1.len != pp2.len) return pp1.len > pp2.len;
            return strncmp(pp1.str, pp2.str, pp1.len) > 0;
        }

        friend bool operator!= (const PathPair &pp1, const PathPair &pp2)
        {
            return !(pp1 == pp2);
        }

    public:
        file_id_t id;
        bool copy;
        char *str;
        int len;
    };

}}}
/*
template <>
struct std::tr1::hash<fos::fs::app::PathPair> {
    public:
        size_t operator()(fos::fs::app::PathPair x) const throw() {
            size_t h = fosPathHashn(x.id, x.str, x.len);
            return h;
        }
};
*/
namespace fos { namespace fs { namespace app {
    typedef std::pair<file_id_t, int> node_pair;
    typedef std::pair<PathPair, InodeType> entry_pair;

    class DirCache
    {
    public:
        DirCache();
        ~DirCache();
        static DirCache & dc() { return *g_dc; }

        int resolve(const InodeType parent, const char *path, int pathlen, InodeType *result, bool *cached);
        void add(const InodeType parent, const char *path, int pathlen, InodeType child);
        void remove(const InodeType parent, const char *path, int pathlen);
        void remove(const InodeType parent, const char *path);
        void remove_dest(Inode child);
        void clear();

    private:
        static DirCache * g_dc;

        //std::tr1::unordered_map<PathPair, DirCacheEntry> m_cache;
        std::map<PathPair, InodeType> m_cache;
    };

}}}

