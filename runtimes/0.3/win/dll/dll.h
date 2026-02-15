#ifndef DLL_PROVIDER_H
#define DLL_PROVIDER_H


#include <winsock2.h>
#include <ws2tcpip.h>
#include "windows.h"


#include "../runtime_lib.h"
#include "../remote.h"
#include "../runtime.h"


struct dll_input_table
{
    int64_t provider;
    int64_t size;
    int64_t param;
    BYTE type;
};

struct dll_worker_info
{
    void *entry;
    char *entryName;
    int64_t call_stack_usage;
    int64_t output_size;
    int64_t inputMapLength;
    struct dll_input_table *inputMap;
};

void dllExecuteWorker(struct queued_worker *obj);
int64_t dllUpdateWaitingWorker(struct waiting_worker *wk, int64_t ticks, int64_t *rdiValue);
void dllStartNewLocalWorker(int64_t workerId, BYTE *inputTable);


#endif
