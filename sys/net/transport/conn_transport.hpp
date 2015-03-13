#pragma once

namespace fos { namespace net { namespace transport {
/*
void transferConnection(void * vpmsg, size_t size, DispatchToken token, void * vptrans);
void provideAppWithConnection(void * vpmsg, size_t size, DispatchToken cm_token, void * vptrans);
*/
void claimConnection(Channel* chan, DispatchTransaction trans, void* msg, size_t size);
}}}
