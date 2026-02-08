#define _WIN32_WINNT _WIN32_WINNT_VISTA

#include <winsock2.h>
#include <ws2tcpip.h>
#include "windows.h"

#include "../runtime_lib.h"
#include "../remote.h"
#include "../runtime.h"

#include "gpu.h"

void gpuPauseWorker(void *returnAddress, void *rbpValue, enum worker_wait_state state, void *state_data)
{
    /* save context and select next worker */
    struct waiting_worker *t = myMalloc(sizeof(*t));

    struct thread_data* lc_data = TlsGetValue(dwTlsIndex);

    memcpy(t->context, context, sizeof(t->context));
    t->id = lc_data->runningId;
    t->data = returnAddress;
    t->state = state;
    t->state_data = state_data;
    t->rbpValue = rbpValue;

    log("Paused worker GPU %lld [cause %lld]\n", lc_data->runningId, (int64_t)state);

    WaitListWorker(t);
}
