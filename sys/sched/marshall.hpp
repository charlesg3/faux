#pragma once

#include <messaging/dispatch.h>

namespace fos { namespace sched {

void exec(Channel*, DispatchTransaction, void*, size_t);

}}


