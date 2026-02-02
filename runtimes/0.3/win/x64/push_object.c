#define _WIN32_WINNT _WIN32_WINNT_VISTA

#include <winsock2.h>
#include <ws2tcpip.h>
#include "windows.h"

#include "../runtime_lib.h"
#include "../remote.h"
#include "../runtime.h"
#include "x64.h"


void x64UpdateLocalPush(void *obj, int64_t offset, int64_t size, void *source)
{
    if (((BYTE *)obj)[-1] == OBJECT_PROMISE)
    {
        struct object_promise *objp = (void *)((int64_t)obj - DATA_OFFSET(*objp));
        objp->ready = 1;
    }
    switch (size)
    {
        case -1:
            ((BYTE *)obj + offset)[0] = (BYTE)(int64_t)source;
            break;
        case -2:
            ((int16_t *)((BYTE *)obj + offset))[0] = (int16_t)(int64_t)source;
            break;
        case -4:
            ((int32_t *)((BYTE *)obj + offset))[0] = (int32_t)(int64_t)source;
            break;
        case -8:
            ((int64_t *)((BYTE *)obj + offset))[0] = (int64_t)(int64_t)source;
            break;
        default:
            memcpy((BYTE *)obj + offset, source, size);
    }
}


__attribute__((sysv_abi))
void x64PushObject(int64_t object_id, void *source, int64_t offset, int64_t size, void *returnAddress, void *rbpValue)
{
    BYTE *obj = (BYTE *)GetHashtable(&local_objects, (BYTE *)&object_id, 8, 0);
    if (obj == 0)
    {
        void *data = myMalloc(myAbs(size));
        memcpy(data, (size < 0 ? &source : source), myAbs(size));
        /* shedule query */
        RequestObjectSet(object_id, offset, myAbs(size), data);
        struct waiting_push *push = myMalloc(sizeof(*push));
        *push = (struct waiting_push){
            .type = WAITING_PUSH,
            .object_id = object_id,
            .size = size,
            .offset = offset,
            .data = data,
            .repeat_timeout = SheduleTimeoutFromNow(PUSH_REPEAT_TIMEOUT),
        };
        BCryptGenRandom(NULL, push->id, BROADCAST_ID_LENGTH, BCRYPT_USE_SYSTEM_PREFERRED_RNG);
        x64PauseWorker(returnAddress, rbpValue, (struct waiting_cause *)push);
    
        struct thread_data* lc_data = TlsGetValue(dwTlsIndex);
        longjmpUN(&lc_data->ShedulerBuffer, 1);
    }
    else
    {
        x64UpdateLocalPush(obj, offset, size, source);
    }
}


__attribute__((sysv_abi))
void x64PushPipe(int64_t object_id, void *source, int64_t offset, int64_t size, void *returnAddress, void *rbpValue)
{
    log("push to pipe %lld\n", object_id);
    ExitProcess(0);
    BYTE *obj = (BYTE *)GetHashtable(&local_objects, (BYTE *)&object_id, 8, 0);
    if (obj == 0)
    {
        void *data = myMalloc(myAbs(size));
        memcpy(data, (size < 0 ? &source : source), myAbs(size));
        /* shedule query */
        RequestObjectSet(object_id, offset, myAbs(size), data);
        struct waiting_push *push = myMalloc(sizeof(*push));
        *push = (struct waiting_push){
            .type = WAITING_PUSH,
            .object_id = object_id,
            .size = size,
            .offset = offset,
            .data = data,
            .repeat_timeout = SheduleTimeoutFromNow(PUSH_REPEAT_TIMEOUT),
        };
        BCryptGenRandom(NULL, push->id, BROADCAST_ID_LENGTH, BCRYPT_USE_SYSTEM_PREFERRED_RNG);
        x64PauseWorker(returnAddress, rbpValue, (struct waiting_cause *)push);
    
        struct thread_data* lc_data = TlsGetValue(dwTlsIndex);
        longjmpUN(&lc_data->ShedulerBuffer, 1);
    }
    else
    {
        x64UpdateLocalPush(obj, offset, size, source);
    }
}

