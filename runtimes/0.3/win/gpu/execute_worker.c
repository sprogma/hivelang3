#define UNICODE 1
#define _UNICODE 1
#define _WIN32_WINNT _WIN32_WINNT_VISTA

#include "inttypes.h"
#include "gpu.h"
#include "../providers.h"


extern int gpuAsmExecuteWorker(void *, int64_t, void *, BYTE *);
void gpuExecuteWorker(struct queued_worker *worker)
{
    if (worker->data == NULL)
    {
        // worker was waiting for push, and now it ends
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
    RequestObjectSet(promise, 0, 8, &value);
    
    // wait for answer

    BYTE *obj = (BYTE *)GetHashtable(&local_objects, (BYTE *)&promise, 8, 0);
    if (obj == NULL)
    {
        void *data = myMalloc(8);
        *(int64_t *)data = value;
        
        struct waiting_push *push = myMalloc(sizeof(*push));
        *push = (struct waiting_push){
            .type = WAITING_PUSH,
            .object_id = promise,
            .size = 8,
            .offset = 0,
            .data = data,
            .repeat_timeout = SheduleTimeoutFromNow(PUSH_REPEAT_TIMEOUT),
        };
        BCryptGenRandom(NULL, push->id, BROADCAST_ID_LENGTH, BCRYPT_USE_SYSTEM_PREFERRED_RNG);
        gpuPauseWorker((struct waiting_cause *)push);

        struct thread_data* lc_data = TlsGetValue(dwTlsIndex);
        longjmpUN(&lc_data->ShedulerBuffer, 1);
    }

    universalUpdateLocalPush(obj, 0, 8, &value);
}
