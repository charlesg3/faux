#ifndef _MESSAGING_DISPATCH_RESET_H
#define _MESSAGING_DISPATCH_RESET_H

#ifdef __cplusplus
extern "C" {
#endif
void dispatchReset();
void dispatcherFree();
void dispatchSetNotifyCallback(void (*callback)(void *, size_t));
#ifdef __cplusplus
}
#endif

#endif

