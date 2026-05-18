#define _WIN32_WINNT _WIN32_WINNT_VISTA

#include "../system.h"

#include "../runtime_lib.h"
#include "../remote.h"
#include "../runtime.h"

#include "x64.h"

struct waiting_worker *x64PauseWorker(void *returnAddress, void *rbpValue, enum worker_wait_state state, void *state_data)
{
    /* save context and select next worker */
    struct waiting_worker *t = AllocateWaitingWorker();
    log("allocated %p\n", t);

    struct thread_data* lc_data = TlsGetValue(dwTlsIndex);

    memcpy(t->context, rbpValue - 1024, sizeof(t->context));
    t->links = 1;
    t->id = lc_data->runningId;
    t->depth = lc_data->runningDepth;
    t->data = returnAddress;
    t->state = state;
    t->state_data = state_data;
    t->rbpValue = rbpValue;

    log("Paused worker %lld [cause %lld]\n", lc_data->runningId, (int64_t)state);

    return t;
}


void x64FreeWaitingWorker(struct waiting_worker *wk)
{
    myFree(wk);
}
