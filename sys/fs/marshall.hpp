#pragma once

#include <messaging/dispatch.h>

namespace fos { namespace fs {

void create(Channel*, DispatchTransaction, void*, size_t);
void destroy(Channel*, DispatchTransaction, void*, size_t);
void addmap(Channel*, DispatchTransaction, void*, size_t);
void removemap(Channel*, DispatchTransaction, void*, size_t);
void pipe(Channel*, DispatchTransaction, void*, size_t);
void open(Channel*, DispatchTransaction, void*, size_t);
void dup(Channel*, DispatchTransaction, void*, size_t);
void read(Channel*, DispatchTransaction, void*, size_t);
void write(Channel*, DispatchTransaction, void*, size_t);
void lseek(Channel*, DispatchTransaction, void*, size_t);
void direntry(Channel*, DispatchTransaction, void*, size_t);
void unlink(Channel*, DispatchTransaction, void*, size_t);
void fstat(Channel*, DispatchTransaction, void*, size_t);
void readlink(Channel*, DispatchTransaction, void*, size_t);
void rename(Channel*, DispatchTransaction, void*, size_t);
void rmdir(Channel*, DispatchTransaction, void*, size_t);
void truncate(Channel*, DispatchTransaction, void*, size_t);
void chmod(Channel*, DispatchTransaction, void*, size_t);
void chown(Channel*, DispatchTransaction, void*, size_t);
void resolve(Channel*, DispatchTransaction, void*, size_t);
void utimens(Channel*, DispatchTransaction, void*, size_t);
void symlink(Channel*, DispatchTransaction, void*, size_t);
void close(Channel*, DispatchTransaction, void*, size_t);
void blocks(Channel*, DispatchTransaction, void*, size_t);
void flush(Channel*, DispatchTransaction, void*, size_t);

void closeCallback(Channel*);

}}

