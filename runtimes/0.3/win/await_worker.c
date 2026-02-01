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

    struct thread_data* lc_data = TlsGetValue(dwTlsIndex);

    memcpy(t->context, context, sizeof(t->context));
    t->id = lc_data->runningId;
    t->ptr = returnAddress;
    t->rbpValue = rbpValue;
    t->waiting_data = waiting_data;

    log("Paused worker %lld [cause %lld]\n", lc_data->runningId, (int64_t)waiting_data->type);

    WaitListWorker(t);
}
