#define _WIN32_WINNT _WIN32_WINNT_VISTA

#include <winsock2.h>
#include <ws2tcpip.h>
#include "windows.h"

#include "runtime_lib.h"
#include "remote.h"
#include "runtime.h"


int64_t QueryLocalObject(void *destination, int64_t object, int64_t offset, int64_t size, int64_t *rdiValue)
{
    if (((BYTE *)object)[-10] == OBJECT_PROMISE)
    {
        struct object_promise *p = (struct object_promise *)(object - DATA_OFFSET(*p));
        if (p->ready)
        {
            if (size < 0)
            {
                memcpy(rdiValue, p->data + offset, -size);
            }
            else
            {
                memcpy(destination, p->data + offset, size);
            }
            return 1;
        }
        return 0;
    }
    else
    {
        struct object_object *p = (struct object_object *)(object - DATA_OFFSET(*p));
        if (size < 0)
        {
            memcpy(rdiValue, p->data + offset, -size);
        }
        else
        {
            memcpy(destination, p->data + offset, size);
        }
        return 1;
    }
}


__attribute__((sysv_abi))
int64_t QueryObject(void *destination, int64_t object, int64_t offset, int64_t size, void *returnAddress, void *rbpValue)
{
    log("Query object %lld\n", object);

    if (((BYTE *)object)[-9] == 0)
    {
        int64_t rdiValue;
        if (QueryLocalObject(destination, object, offset, size, &rdiValue))
        {
            return rdiValue;
        }
    }
    else
    {
        // send request
        RequestObjectGet(object, offset, myAbs(size));
    }
    /* shedule query */
    struct waiting_query query = {
        .destination = destination,
        .object = object,
        .size = size,
        .offset = offset,
    };
    PauseWorker(returnAddress, rbpValue, (struct waiting_cause *)&query);
    longjmpUN(&ShedulerBuffer, 1);
}
