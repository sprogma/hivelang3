#define UNICODE 1
#define _UNICODE 1
#define _WIN32_WINNT _WIN32_WINNT_VISTA

#include "inttypes.h"
#include "gpu.h"
#include "../providers.h"
#include "../x64/x64.h"


void gpuExecuteWorker(struct queued_worker *worker)
{
    if (worker->data == RETURN_STATE_GPU_END)
    {
        return;
    }
    // TODO: set interrupt lock
    myPrintf(L"RUN GPU worker=%p\n", worker);
    
    BYTE *inputTable = (BYTE *)worker->rdiValue;
    struct gpu_worker_info *info = Workers[worker->id].data;
    int64_t inputTableSize = Workers[worker->id].inputSize;

    /* execute worker */
    (void)info;

    /* after return: push Errorcode to promise */
    int64_t value = 0; // TODO: set error code instead of 0
    
    int64_t promise = *(int64_t *)&inputTable[inputTableSize - 8];
   
    // wait for answer

    x64PushObject(promise, &value, 0, 8, RETURN_STATE_GPU_END, NULL);

    // return
}
