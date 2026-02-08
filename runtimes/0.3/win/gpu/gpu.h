#ifndef GPU_PROVIDER_H
#define GPU_PROVIDER_H


#include <winsock2.h>
#include <ws2tcpip.h>
#include "windows.h"


#include "../runtime_lib.h"
#include "../gpu_subsystem.h"
#include "../remote.h"
#include "../runtime.h"


struct gpu_worker_info
{
    char *start;
    char *end;
    int64_t inputMapLength;
    BYTE *inputMap;
};

struct gpu_object
{
    int64_t size;
    int64_t length; // for arrays
    cl_mem mem;
};


#define RETURN_STATE_GPU_END ((void *)1)


void gpuExecuteWorker(struct queued_worker *obj);
int64_t gpuUpdateWaitingWorker(struct waiting_worker *wk, int64_t ticks, int64_t *rdiValue);
void gpuNewObjectUsingPage(int64_t type, int64_t size, int64_t param, int64_t *remote_id);


#endif
