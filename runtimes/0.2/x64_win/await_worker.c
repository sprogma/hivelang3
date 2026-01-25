#define _WIN32_WINNT _WIN32_WINNT_VISTA

#include <winsock2.h>
#include <ws2tcpip.h>
#include "windows.h"

#include "runtime_lib.h"
#include "remote.h"
#include "runtime.h"



void PauseWorker(void *returnAddress, void *rbpValue, struct waiting_cause *waiting_data)
{
    /* save context and select next worker */
    struct waiting_worker *t = myMalloc(sizeof(*t));

    memcpy(t->context, context, sizeof(t->context));
    t->id = runningId;
    t->ptr = returnAddress;
    t->rbpValue = rbpValue;
    t->waiting_data = waiting_data;

    log("Paused worker %lld [cause %lld]\n", runningId, (int64_t)waiting_data->type);

    WaitListWorker(t);
}
