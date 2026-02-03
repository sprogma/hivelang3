#define _WIN32_WINNT _WIN32_WINNT_VISTA

#include <winsock2.h>
#include <ws2tcpip.h>
#include "windows.h"

#include "../runtime_lib.h"
#include "../remote.h"
#include "../runtime.h"
#include "x64.h"

void x64PauseWorker(void *returnAddress, void *rbpValue, enum worker_wait_state state, void *state_data)
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

    log("Paused worker %lld [cause %lld]\n", lc_data->runningId, (int64_t)state);

    WaitListWorker(t);
}

int64_t x64UpdateWaitingWorker(struct waiting_worker *w, int64_t ticks, int64_t *rdiValue)
{
    switch (w->waiting_data->type)
    {
        case WAITING_PUSH:
        {
            struct waiting_push *cause = (struct waiting_push *)w->waiting_data;
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
                break;
            }
            else
            {
                log("ERROR: waiting for push to local\n");
                break;
            }
            break;
        }
        case WAITING_QUERY:
        {
            struct waiting_query *cause = (struct waiting_query *)w->waiting_data;
            struct object *obj = (void *)GetHashtable(&local_objects, (BYTE *)&cause->object_id, 8, 0);
            if (obj == 0)
            {
                // remote object, repeat request, with timeout
                log("waiting for remote query of %lld [%lld/%lld]\n", cause->object_id, ticks, cause->repeat_timeout);
                if (ticks > cause->repeat_timeout)
                {
                    RequestObjectGet(cause->object_id, cause->offset, myAbs(cause->size));
                    cause->repeat_timeout = SheduleTimeoutFromNow(QUERY_REPEAT_TIMEOUT);
                }
                break;
            }
            else
            {
                log("waiting for local %lld\n", cause->object_id);
                int64_t rdiValue;
                if (x64QueryLocalObject(cause->destination, obj, cause->offset, cause->size, &rdiValue))
                {
                    myFree(cause);
                    return 1;
                }
                break;
            }
            break;
        }
        case WAITING_TIMER:
            print("NOT IMPLEMENTED WAITING_TIMER\n"); break;
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
            break;
        }
    }
    return 0;
}
