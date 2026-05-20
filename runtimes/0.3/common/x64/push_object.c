#define _WIN32_WINNT _WIN32_WINNT_VISTA

#include "../system.h"

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
    int64_t hash;
    void *data;
    BYTE id[BROADCAST_ID_LENGTH];
    int64_t repeat_timeout;
};
//@regPush WK_STATE_PUSH_OBJECT_WAIT_X64 x64OnPushObject
int64_t x64OnPushObject(struct waiting_worker *w, int64_t object, int64_t offset, int64_t size, int64_t hash)
{
    switch (w->state)
    {
    
    case WK_STATE_PUSH_OBJECT_WAIT_X64:
        struct wait_push_info *info = w->state_data;
        if (info->object_id == object && info->offset == offset && myAbs(info->size) == size && info->hash == hash)
        {
            myFree(info);
            return 1;
        }

        // return object to linked list
        struct set_wait_list_key key = { info->object_id, info->offset, myAbs(info->size), info->hash };

        struct set_wait_list_value *new_value = myMalloc(sizeof(*new_value));
        new_value->params = (void *)w;
        new_value->callback = callbackContinueWorkerFromWaitingPush;

        GetsetInsertTagged(&set_wait_list, &key, new_value);
        
        return 0;
        
    }
    unreachable;
}
//@reg WK_STATE_PUSH_OBJECT_WAIT_X64 x64PushObjectStates
int64_t x64PushObjectStates(struct waiting_worker *w, int64_t ticks, int64_t *rdiValue)
{
    (void)rdiValue;
    switch (w->state)
    {
    
    case WK_STATE_PUSH_OBJECT_WAIT_X64:
        struct wait_push_info *info = w->state_data;
        struct object *obj = (void *)i64GetHashtable(&local_objects, info->object_id);
        if (obj == 0)
        {
            // remote object, repeat request, with timeout
            log("waiting for remote push %lld/%lld for obj=%lld\n", ticks, info->repeat_timeout, info->object_id);
            if (ticks > info->repeat_timeout)
            {
                RequestObjectSet(info->object_id, info->offset, myAbs(info->size), info->data, w);
                info->repeat_timeout = SheduleTimeoutFromNow(PUSH_REPEAT_TIMEOUT);
            }
            else
            {
                // return object to linked list
                struct set_wait_list_key key = { info->object_id, info->offset, myAbs(info->size), info->hash };

                struct set_wait_list_value *new_value = myMalloc(sizeof(*new_value));
                new_value->params = (void *)w;
                new_value->callback = callbackContinueWorkerFromWaitingPush;
                
                GetsetInsertTagged(&set_wait_list, &key, new_value);
            }
            return 0;
        }
        else
        {
            x64UpdateLocalPush(obj, info->offset, info->size, info->data);
            
            if (((BYTE *)obj)[-1] == OBJECT_PROMISE)
            {
                int64_t abssize = myAbs(info->size);
                UpdateWaitingQuery(info->object_id, info->offset, abssize, info->data);
            }
            
            myFree(info);
            return 1;
        }
        unreachable;
    }
    unreachable;
}


__attribute__((sysv_abi))
void x64PushObject(int64_t object_id, void *source, int64_t offset, int64_t size, void *returnAddress, void *rbpValue)
{
    log("push to object %p\n", object_id);
    BYTE *obj = (BYTE *)i64GetHashtable(&local_objects, object_id);
    if (obj == 0)
    {
        struct thread_data* lc_data = TlsGetValue(dwTlsIndex);
        lc_data->stallable = 0;
        
        void *data = myMalloc(myAbs(size));
        memcpy(data, (size < 0 ? &source : source), myAbs(size));
        
        /* pause worker */
        struct wait_push_info *info = myMalloc(sizeof(*info));
        *info = (struct wait_push_info){
            .object_id = object_id,
            .size = size,
            .offset = offset,
            .data = data,
            .hash = GetByteStringHash(data, myAbs(size)),
            .repeat_timeout = SheduleTimeoutFromNow(PUSH_REPEAT_TIMEOUT),
        };
        SECURE_RANDOM(info->id, BROADCAST_ID_LENGTH);
        struct waiting_worker *wait = universalPauseWorker(returnAddress, rbpValue, WK_STATE_PUSH_OBJECT_WAIT_X64, info);
        
        /* shedule query */
        RequestObjectSet(object_id, offset, myAbs(size), data, wait);
    
        longjmpUN(&lc_data->ShedulerBuffer, 1);
    }
    else
    {
        x64UpdateLocalPush(obj, offset, size, source);

        if (((BYTE *)obj)[-1] == OBJECT_PROMISE)
        {
            int64_t abssize = myAbs(size);
            UpdateWaitingQuery(object_id, offset, abssize, (size < 0 ? &source : source));
        }
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


