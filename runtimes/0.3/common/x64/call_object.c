#define _WIN32_WINNT _WIN32_WINNT_VISTA

#include "../system.h"

#include "../runtime_lib.h"
#include "../remote.h"
#include "../runtime.h"

#include "x64.h"

__attribute__((sysv_abi))
void x64CallObject(int64_t moditifer, BYTE *args, int64_t workerId, int64_t _, void *returnAddress, void *rbpValue)
{
    (void)returnAddress;
    (void)rbpValue;
    
    int64_t tableSize = Workers[workerId].inputSize;

    log("Calling worker %lld [data=%p, mod=%lld]\n", workerId, args, moditifer);
    log("Table = ");
    for (int64_t i = 0; i < tableSize; ++i)
    {
        log("%02x ", args[i]);
    }
    log("\n");

    StartNewWorker(workerId, moditifer, args);
}

void x64StartNewLocalWorker(int64_t workerId, BYTE *inputTable)
{
    struct x64_worker_data *wk_data = Workers[workerId].data;

    int64_t tableSize = Workers[workerId].inputSize;
    void *data;

    int32_t expect = 0;
    while (!atomic_compare_exchange_weak(&wk_data->spinlock, &expect, 1))
    {
        _mm_pause();
        expect = 0;
    }
    
    if (wk_data->nextBuffer != NULL)
    {
        data = wk_data->nextBuffer;
        wk_data->nextBuffer = *(void **)wk_data->nextBuffer;
        
        atomic_store_explicit(&wk_data->spinlock, 0, memory_order_release);    
    }
    else
    {
        atomic_store_explicit(&wk_data->spinlock, 0, memory_order_release);
        
        // TODO: remove 2048 body size constant
        data = myMalloc(1024 + 2048);
    }

    memcpy(data + 1024 - tableSize, inputTable, tableSize);

    struct thread_data* lc_data = TlsGetValue(dwTlsIndex);
    
    struct queued_worker *t = AllocateQueuedWorker();
    t->id = workerId;
    t->depth = lc_data ? lc_data->runningDepth : 0; // (lc_data ? lc_data->runningDepth + 1 : 0);
    t->data = wk_data->start;
    t->rdiValue = (int64_t)data + 1024 - tableSize;
    t->rbpValue = (BYTE *)data + 1024;
    memset(t->context, 0, sizeof(t->context));
    scheduler_enqueue(&glb_scheduler, 1, t, lc_data ? lc_data->number : 0);
}
