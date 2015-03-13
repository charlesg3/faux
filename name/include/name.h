#ifndef _NAME_H
#define _NAME_H

#include <messaging/channel_interface.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint64_t Name;

void name_init();
void name_cleanup();
void name_reset(); // don't send the close message

void name_register(Name name, Address ep_addr);
void name_unregister(Name name, Address ep_addr);
bool name_lookup(Name name, Address *ep_addr);

#ifdef __cplusplus
}
#endif

#endif // _NAME_H

