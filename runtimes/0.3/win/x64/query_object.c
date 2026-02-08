#define _WIN32_WINNT _WIN32_WINNT_VISTA

#include <winsock2.h>
#include <ws2tcpip.h>
#include "windows.h"

#include "../runtime_lib.h"
#include "../remote.h"
#include "../runtime.h"
#include "x64.h"


int64_t x64QueryLocalObject(void *destination, void *object, int64_t offset, int64_t size, int64_t *rdiValue)
{
    if (((BYTE *)object)[-1] == OBJECT_PROMISE)
    {
        struct object_promise *p = (struct object_promise *)(object - DATA_OFFSET(*p));
        if (p->ready)
        {
            memcpy((size < 0 ? rdiValue : destination), p->data + offset, myAbs(size));
            return 1;
        }
        return 0;
    }
    else
    {
        struct object_object *p = (struct object_object *)(object - DATA_OFFSET(*p));
        memcpy((size < 0 ? rdiValue : destination), p->data + offset, myAbs(size));
        return 1;
    }
}


struct wait_query_info
{
    int64_t object_id; 
    int64_t offset;
    int64_t size;
    void *destination;
    BYTE id[BROADCAST_ID_LENGTH];
    int64_t repeat_timeout;
};

//@regQuery WK_STATE_QUERY_OBJECT_WAIT_X64 x64OnQueryObject

int64_t x64OnQueryObject(struct waiting_worker *w, int64_t object, int64_t offset, int64_t size, void *data, int64_t *rdiValue)
{
    switch (w->state)
    {
        
    case WK_STATE_QUERY_OBJECT_WAIT_X64:
    {
        struct wait_query_info *info = w->state_data;
        if (info->object_id == object && myAbs(info->size) == size && info->offset == offset)
        {
            UpdateFromQueryResult(info->destination, object, offset, info->size, data, rdiValue);
            myFree(info);
            return 1;
        }
        return 0;
    }
        
    }
    __builtin_unreachable();
}


//@reg WK_STATE_QUERY_OBJECT_WAIT_X64 x64QueryObjectStates
int64_t x64QueryObjectStates(struct waiting_worker *w, int64_t ticks, int64_t *rdiValue)
{
    switch (w->state)
    {
        
    case WK_STATE_QUERY_OBJECT_WAIT_X64:
        struct wait_query_info *info = w->state_data;
        struct object *obj = (void *)GetHashtable(&local_objects, (BYTE *)&info->object_id, 8, 0);
        if (obj == 0)
        {
            // remote object, repeat request, with timeout
            log("waiting for remote query %lld/%lld\n", ticks, info->repeat_timeout);
            if (ticks > info->repeat_timeout)
            {                   
                RequestObjectGet(info->object_id, info->offset, myAbs(info->size));
                info->repeat_timeout = SheduleTimeoutFromNow(QUERY_REPEAT_TIMEOUT);
            }
            break;
        }
        else
        {
            if (x64QueryLocalObject(info->destination, obj, info->offset, info->size, rdiValue))
            {
                myFree(info);
                return 1;
            }
        }
        return 0;
        
    }
    __builtin_unreachable();
}


__attribute__((sysv_abi))
int64_t x64QueryObject(void *destination, int64_t object_id, int64_t offset, int64_t size, void *returnAddress, void *rbpValue)
{
    log("Query object %lld\n", object_id);

    BYTE *obj = (BYTE *)GetHashtable(&local_objects, (BYTE *)&object_id, 8, 0);

    if (obj != NULL)
    {
        int64_t rdiValue;
        if (x64QueryLocalObject(destination, obj, offset, size, &rdiValue))
        {
            return rdiValue;
        }
    }
    else
    {
        // send request
        RequestObjectGet(object_id, offset, myAbs(size));
    }
    /* shedule query */
    struct wait_query_info *query = myMalloc(sizeof(*query));
    *query = (struct wait_query_info){
        .object_id = object_id,
        .offset = offset,
        .size = size,
        .destination = destination,
        .repeat_timeout = SheduleTimeoutFromNow(QUERY_REPEAT_TIMEOUT),
    };
    BCryptGenRandom(NULL, query->id, BROADCAST_ID_LENGTH, BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    universalPauseWorker(returnAddress, rbpValue, WK_STATE_QUERY_OBJECT_WAIT_X64, query);
    
    struct thread_data* lc_data = TlsGetValue(dwTlsIndex);
    longjmpUN(&lc_data->ShedulerBuffer, 1);
}


__attribute__((sysv_abi))
int64_t x64QueryPipe(void *destination, int64_t object_id, int64_t offset, int64_t size, void *returnAddress, void *rbpValue)
{
    (void)destination;
    (void)offset;
    (void)size;
    (void)returnAddress;
    (void)rbpValue;
    log("query from pipe %lld\n", object_id);
    ExitProcess(0);
}
