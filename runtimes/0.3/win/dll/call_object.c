#define _WIN32_WINNT _WIN32_WINNT_VISTA

#include <winsock2.h>
#include <ws2tcpip.h>
#include "windows.h"

#include "../runtime_lib.h"
#include "../remote.h"
#include "../runtime.h"

#include "dll.h"

__attribute__((sysv_abi))
void dllCallObject(int64_t moditifer, BYTE *args, int64_t workerId, int64_t _, void *returnAddress, void *rbpValue)
{
    (void)returnAddress;
    (void)rbpValue;
    
    int64_t tableSize = Workers[workerId].inputSize;

    log("Calling dllimport worker %lld [data=%p, mod=%lld]\n", workerId, args, moditifer);
    log("Table = ");
    for (int64_t i = 0; i < tableSize; ++i)
    {
        log("%02x ", args[i]);
    }
    log("\n");

    StartNewWorker(workerId, moditifer, args);
}

void dllStartNewLocalWorker(int64_t workerId, BYTE *inputTable)
{
    // default worker startup
    
    int64_t tableSize = Workers[workerId].inputSize;
    void *data = myMalloc(1024 + 2048);
    memcpy(data + 1024 - tableSize, inputTable, tableSize);

    struct thread_data* lc_data = TlsGetValue(dwTlsIndex);
    
    struct queued_worker *t = myMalloc(sizeof(*t));
    t->id = workerId;
    t->depth = (lc_data ? lc_data->runningDepth + 1 : 0);
    t->data = Workers[workerId].data;
    t->rdiValue = (int64_t)data + 1024 - tableSize;
    t->rbpValue = (BYTE *)data + 1024;
    memset(t->context, 0, sizeof(t->context));
    queue_enqueue(t);
}
