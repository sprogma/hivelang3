#define UNICODE 1
#define _UNICODE 1
#define _WIN32_WINNT _WIN32_WINNT_VISTA

#include "inttypes.h"
#include "gpu.h"


extern int gpuAsmExecuteWorker(void *, int64_t, void *, BYTE *);
void gpuExecuteWorker(struct queued_worker *worker)
{
    // TODO: set interrupt lock
    myPrintf(L"RUN GPU worker=%p\n", worker);
    
    BYTE *inputTable = (BYTE *)worker->rdiValue;
    struct gpu_worker_info *info = Workers[worker->id].data;
    int64_t inputTableSize = Workers[worker->id].inputSize;

    /* after return: push Errorcode to promise */
    int64_t promise = *(int64_t *)inputTable[inputTableSize - 8];
    int64_t value = 0; // TODO: set error code instead of 0
    RequestObjectSet(promise, 0, 8, &value);
    // wait for answer
    gpuPauseWorker(worker);

    ExitProcess(0);
}
