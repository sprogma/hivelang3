#ifndef X64_PROVIDER_H
#define X64_PROVIDER_H


#include <winsock2.h>
#include <ws2tcpip.h>
#include "windows.h"


#include "../runtime_lib.h"
#include "../remote.h"
#include "../runtime.h"


void x64ExecuteWorker(struct queued_worker *obj);
void x64PauseWorker(void *returnAddress, void *rbpValue, struct waiting_cause *waiting_data);
int64_t x64UpdateWaitingWorker(struct waiting_worker *wk, int64_t ticks, int64_t *rdiValue);
void x64NewObjectUsingPage(int64_t type, int64_t size, int64_t param, int64_t remote_id);
int64_t x64QueryLocalObject(void *destination, void *object, int64_t offset, int64_t size, int64_t *rdiValue);
void x64UpdateLocalPush(void *obj, int64_t offset, int64_t size, void *source);


#endif
