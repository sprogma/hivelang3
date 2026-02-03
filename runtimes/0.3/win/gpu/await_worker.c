#define _WIN32_WINNT _WIN32_WINNT_VISTA

#include <winsock2.h>
#include <ws2tcpip.h>
#include "windows.h"

#include "../runtime_lib.h"
#include "../remote.h"
#include "../runtime.h"

#include "gpu.h"

void gpuPauseWorker(struct waiting_cause *waiting_data)
{
    /* save context and select next worker */
    struct waiting_worker *t = myMalloc(sizeof(*t));

    struct thread_data* lc_data = TlsGetValue(dwTlsIndex);

    memcpy(t->context, context, sizeof(t->context));
    t->id = lc_data->runningId;
    t->data = NULL;
    t->rbpValue = 0;
    t->waiting_data = (void *)waiting_data;

    log("Paused worker GPU %lld\n", lc_data->runningId);

    WaitListWorker(t);
}

int64_t gpuUpdateWaitingWorker(struct waiting_worker *w, int64_t ticks, int64_t *rdiValue)
{
    (void)rdiValue;
    
    switch (w->waiting_data->type)
    {
        case WAITING_PUSH:
        {
            struct waiting_push *cause = (void *)w->waiting_data;        
            struct object *obj = (void *)GetHashtable(&local_objects, (BYTE *)&cause->object_id, 8, 0);
            if (obj == 0)
            {
                // remote object, repeat request, with timeout
                log("waiting for remote push %lld/%lld\n", ticks, cause->repeat_timeout);
                if (ticks > cause->repeat_timeout)
                {
                    RequestObjectSet(cause->object_id, cause->offset, myAbs(cause->size), cause->data);
                    cause->repeat_timeout = SheduleTimeoutFromNow(PUSH_REPEAT_TIMEOUT);
                }
                return 0;
            }
            else
            {
                log("ERROR: waiting for push to local\n");
                return 0;
            }
        }
        case WAITING_PAGES:
        {
            struct waiting_pages *cause = (struct waiting_pages *)w->waiting_data;
            int64_t new_id;
            if (GetNewObjectId(&new_id))
            {
                Providers[cause->provider].NewObjectUsingPage(cause->obj_type, cause->size, cause->param, new_id);
                *rdiValue = new_id;
                myFree(cause);
                return 1;
            }
            return 0;
        }
        default:
            return 0;
    }
}
