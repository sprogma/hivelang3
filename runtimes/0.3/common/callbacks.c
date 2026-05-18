#include "remote.h"
#include "runtime.h"


void callbackQueryAnswerLocalId(int64_t object_id, int64_t offset, int64_t size, BYTE *data, void *params)
{
    int64_t local_id = (int64_t)params;
    AnswerQueryObject(GetConnectionById(local_id, NULL), data, object_id, offset, size);
}

void callbackPushAnswerLocalId(int64_t object_id, int64_t offset, int64_t size, int64_t hash, void *params)
{
    int64_t local_id = (int64_t)params;
    AnswerPushObject(GetConnectionById(local_id, NULL), object_id, offset, size, hash);
}


void callbackContinueWorkerFromWaitingQuery(int64_t object_id, int64_t offset, int64_t size, BYTE *data, void *params)
{
    struct waiting_worker *w = params;
    log("update worker: %p\n", w);
    int64_t res = 0, rdiValue;
    switch (w->state)
    {
        //<<--Quote-->> from::(ls *.c -r|sls "^\s*//@regQuery\s+(\w+)\s+(\w+)$"|% Matches|%{[pscustomobject]@{a=$_.Groups[1];b=$_.Groups[2]}}|group b|%{$n=$_;$_.Group|%{"$(" "*16)int64_t $($n.Name)(struct waiting_worker *, int64_t, int64_t, int64_t, void *, int64_t *);"}})-join"`n"
        int64_t castOnQueryObject(struct waiting_worker *, int64_t, int64_t, int64_t, void *, int64_t *);
        int64_t castOnQueryObject(struct waiting_worker *, int64_t, int64_t, int64_t, void *, int64_t *);
        int64_t x64OnQueryObject(struct waiting_worker *, int64_t, int64_t, int64_t, void *, int64_t *);
        //<<--QuoteEnd-->>
        //<<--Quote-->> from::(ls *.c -r|sls "^\s*//@regQuery\s+(\w+)\s+(\w+)$"|% Matches|%{[pscustomobject]@{a=$_.Groups[1];b=$_.Groups[2]}}|group b|%{$n=$_;$_.Group|%{"$(" "*16)case $($_.a):"};"$(" "*20)res = $($n.Name)(w, object_id, offset, size, data, &rdiValue); break;"})-join"`n"
        case WK_STATE_GET_OBJECT_SIZE:
        case WK_STATE_GET_OBJECT_DATA:
        res = castOnQueryObject(w, object_id, offset, size, data, &rdiValue); break;
        case WK_STATE_QUERY_OBJECT_WAIT_X64:
        res = x64OnQueryObject(w, object_id, offset, size, data, &rdiValue); break;
        //<<--QuoteEnd-->>
    }
    if (res)
    {
        EnqueueWorkerFromWaitList(w, rdiValue);
        int64_t tmp = atomic_fetch_sub(&w->links, 1);
        assert(tmp >= 1);
        if (tmp == 1)
        {
            FreeWaitingWorker(w);
        }
    }
}


void callbackContinueWorkerFromWaitingPush(int64_t object_id, int64_t offset, int64_t size, int64_t hash, void *params)
{
    struct waiting_worker *w = params;
    log("update worker: %p\n", w);
    int64_t res = 0;
    switch (w->state)
    {
        //<<--Quote-->> from::(ls *.c -r|sls "^\s*//@regPush\s+(\w+)\s+(\w+)$"|% Matches|%{[pscustomobject]@{a=$_.Groups[1];b=$_.Groups[2]}}|group b|%{$n=$_;$_.Group|%{"$(" "*8)int64_t $($n.Name)(struct waiting_worker *, int64_t, int64_t, int64_t, int64_t);"}})-join"`n"
        int64_t x64OnPushObject(struct waiting_worker *, int64_t, int64_t, int64_t, int64_t);
        //<<--QuoteEnd-->>
        //<<--Quote-->> from::(ls *.c -r|sls "^\s*//@regPush\s+(\w+)\s+(\w+)$"|% Matches|%{[pscustomobject]@{a=$_.Groups[1];b=$_.Groups[2]}}|group b|%{$n=$_;$_.Group|%{"        case $($_.a):"};"            res = $($n.Name)(w, object_id, offset, size, hash); break;"})-join"`n"
        case WK_STATE_PUSH_OBJECT_WAIT_X64:
        res = x64OnPushObject(w, object_id, offset, size, hash); break;
        //<<--QuoteEnd-->>
    }
    if (res == 1)
    {
        EnqueueWorkerFromWaitList(w, 0);

        int64_t tmp = atomic_fetch_sub(&w->links, 1);
        assert(tmp >= 1);
        if (tmp == 1)
        {
            FreeWaitingWorker(w);
        }
    }
}

