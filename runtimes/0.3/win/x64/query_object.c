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
    struct waiting_query *query = myMalloc(sizeof(*query));
    *query = (struct waiting_query){
        .type = WAITING_QUERY,
        .destination = destination,
        .object_id = object_id,
        .size = size,
        .offset = offset,
        .repeat_timeout = SheduleTimeoutFromNow(QUERY_REPEAT_TIMEOUT),
    };
    BCryptGenRandom(NULL, query->id, BROADCAST_ID_LENGTH, BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    x64PauseWorker(returnAddress, rbpValue, (struct waiting_cause *)query);
    
    struct thread_data* lc_data = TlsGetValue(dwTlsIndex);
    longjmpUN(&lc_data->ShedulerBuffer, 1);
}


__attribute__((sysv_abi))
int64_t x64QueryPipe(void *destination, int64_t object_id, int64_t offset, int64_t size, void *returnAddress, void *rbpValue)
{
    log("query from pipe %lld\n", object_id);
    ExitProcess(0);

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
    struct waiting_query *query = myMalloc(sizeof(*query));
    *query = (struct waiting_query){
        .type = WAITING_QUERY,
        .destination = destination,
        .object_id = object_id,
        .size = size,
        .offset = offset,
        .repeat_timeout = SheduleTimeoutFromNow(QUERY_REPEAT_TIMEOUT),
    };
    BCryptGenRandom(NULL, query->id, BROADCAST_ID_LENGTH, BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    x64PauseWorker(returnAddress, rbpValue, (struct waiting_cause *)query);
    
    struct thread_data* lc_data = TlsGetValue(dwTlsIndex);
    longjmpUN(&lc_data->ShedulerBuffer, 1);
}
