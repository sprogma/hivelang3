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


struct wait_push_info
{
    int64_t object_id; 
    int64_t offset;
    int64_t size;
    void *data;
    BYTE id[BROADCAST_ID_LENGTH];
    int64_t repeat_timeout;
};
//@regPush WK_STATE_PUSH_OBJECT_WAIT_X64 x64OnPushObject
int64_t x64OnPushObject(struct waiting_worker *w, int64_t object, int64_t offset, int64_t size)
{
    switch (w->state)
    {
    
    case WK_STATE_PUSH_OBJECT_WAIT_X64:
        struct wait_push_info *info = w->state_data;
        if (info->object_id == object && info->offset == offset && myAbs(info->size) == size)
        {
            myFree(info);
            return 1;
        }
        return 0;
        
    }
    __builtin_unreachable();
}
//@reg WK_STATE_PUSH_OBJECT_WAIT_X64 x64PushObjectStates
int64_t x64PushObjectStates(struct waiting_worker *w, int64_t ticks, int64_t *rdiValue)
{
    (void)rdiValue;
    switch (w->state)
    {
    
    case WK_STATE_PUSH_OBJECT_WAIT_X64:
        struct wait_push_info *info = w->state_data;
        struct object *obj = (void *)GetHashtable(&local_objects, (BYTE *)&info->object_id, 8, 0);
        if (obj == 0)
        {
            // remote object, repeat request, with timeout
            log("waiting for remote push %lld/%lld\n", ticks, info->repeat_timeout);
            if (ticks > info->repeat_timeout)
            {
                RequestObjectSet(info->object_id, info->offset, myAbs(info->size), info->data);
                info->repeat_timeout = SheduleTimeoutFromNow(PUSH_REPEAT_TIMEOUT);
            }
            break;
        }
        else
        {
            x64UpdateLocalPush(obj, info->offset, info->size, info->data);
            myFree(info);
            return 1;
        }
        return 0;
        
    }
    __builtin_unreachable();
}


__attribute__((sysv_abi))
void x64PushObject(int64_t object_id, void *source, int64_t offset, int64_t size, void *returnAddress, void *rbpValue)
{
    log("push to object %p\n", object_id);
    BYTE *obj = (BYTE *)GetHashtable(&local_objects, (BYTE *)&object_id, 8, 0);
    if (obj == 0)
    {
        void *data = myMalloc(myAbs(size));
        memcpy(data, (size < 0 ? &source : source), myAbs(size));
        /* shedule query */
        RequestObjectSet(object_id, offset, myAbs(size), data);
        struct wait_push_info *info = myMalloc(sizeof(*info));
        *info = (struct wait_push_info){
            .object_id = object_id,
            .size = size,
            .offset = offset,
            .data = data,
            .repeat_timeout = SheduleTimeoutFromNow(PUSH_REPEAT_TIMEOUT),
        };
        BCryptGenRandom(NULL, info->id, BROADCAST_ID_LENGTH, BCRYPT_USE_SYSTEM_PREFERRED_RNG);
        universalPauseWorker(returnAddress, rbpValue, WK_STATE_PUSH_OBJECT_WAIT_X64, info);
    
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
    (void)source;
    (void)offset;
    (void)size;
    (void)returnAddress;
    (void)rbpValue;
    log("push to pipe %lld\n", object_id);
    ExitProcess(0);
}

