#define _WIN32_WINNT _WIN32_WINNT_VISTA

#include <winsock2.h>
#include <ws2tcpip.h>
#include "windows.h"

#include "runtime_lib.h"
#include "remote.h"
#include "runtime.h"


// static int64_t QueryObjectLocal()
// {
//     
// }


__attribute__((sysv_abi))
int64_t QueryObject(void *destination, int64_t object_id, int64_t offset, int64_t size, void *returnAddress, void *rbpValue)
{
    /* save context and select next worker */
    struct waiting_worker *t = myMalloc(sizeof(*t));

    memcpy(t->context, context, sizeof(t->context));
    t->id = runningId;
    t->ptr = returnAddress;
    t->rbpValue = rbpValue;
    t->size = size;
    t->offset = offset;
    t->destination = destination;
    t->object = object_id;

    log("Awaiting object at %p\n", object_id);

    WaitListWorker(t);

    longjmpUN(&ShedulerBuffer, 1);

    return 0;
//     int64_t page = object_id >> 24;
//     int64_t lay1 = 
//     /* check local objects */
// 
//     GetHashtable(&local_objects, );
}
