#ifndef GPU_PROVIDER_H
#define GPU_PROVIDER_H


#include <winsock2.h>
#include <ws2tcpip.h>
#include "windows.h"


#include "../runtime_lib.h"
#include "../remote.h"
#include "../runtime.h"


struct gpu_worker_info
{
    char *start;
    char *end;
    int64_t inputMapLength;
    BYTE *inputMap;
};


void gpuExecuteWorker(struct queued_worker *obj);
void gpuPauseWorker(void *returnAddress, void *rbpValue, struct waiting_cause *waiting_data);
int64_t gpuUpdateWaitingWorker(struct waiting_worker *wk, int64_t ticks, int64_t *rdiValue);


#endif
