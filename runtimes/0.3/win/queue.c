#define UNICODE 1
#define _UNICODE 1
#define _WIN32_WINNT _WIN32_WINNT_VISTA

#include <winsock2.h>
#include <ws2tcpip.h>
#include "windows.h"

#include "runtime_lib.h"
#include "runtime.h"

_Atomic int64_t queue_size;

struct queue_t
{
    SRWLOCK queue_lock;
    int64_t size;
    int64_t alloc;
    void **data;
};

// #define USE_ARRAY

#define SHARED_QUEUE ((sizeof(queues) / sizeof(*queues)) - 1)
struct queue_t queues[32];


static inline int p(int x)
{
    return (x - 1) / 2;
}


int64_t is_less(struct queued_worker *w1, struct queued_worker *w2)
{
    return w1->id < w2->id;
}


void push_up(int64_t x)
{   
    void **a = queues[SHARED_QUEUE].data;
    while (x != 0 && is_less(a[p(x)], a[x]))
    {
        void *tmp = a[x];
        a[x] = a[p(x)];
        a[p(x)] = tmp;
        x = p(x);
    }
}


void push_down(int64_t x)
{
    void **a = queues[SHARED_QUEUE].data;
    int64_t n = queues[SHARED_QUEUE].size;
    while (x < n)
    {
        int64_t l = 2 * x + 1;
        int64_t r = 2 * x + 2;
        if (r < n)
        {
            if (is_less(a[l], a[r]))
            {
                l = r;
            }
            if (is_less(a[x], a[l]))
            {
                void *tmp = a[l];
                a[l] = a[x];
                a[x] = tmp;
                x = l;
                continue;
            }
        }
        else if (l < n)
        {
            if (is_less(a[x], a[l]))
            {
                void *tmp = a[l];
                a[l] = a[x];
                a[x] = tmp;
                x = l;
                continue;
            }
        }
        return;
    }
}


void queue_init()
{   
    for (int i = 0; i < (int)(sizeof(queues)/sizeof(*queues)); ++i)
    {
        queues[i].size = 0;
        queues[i].alloc = 1024*16;
        queues[i].data = myMalloc(sizeof(*queues[i].data) * queues[i].alloc);
        queues[i].queue_lock = (SRWLOCK)SRWLOCK_INIT;
    }
    queue_size = 0;
}

void queue_enqueue(struct queued_worker *wk)
{
    queue_size++;
    int64_t queue_id = SHARED_QUEUE;
    if (Workers[wk->id].affinity != -1)
    {
        queue_id = Workers[wk->id].affinity % NUM_THREADS;
    }
    AcquireSRWLockExclusive(&queues[queue_id].queue_lock);
    #ifdef USE_ARRAY
    queues[queue_id].data[queues[queue_id].size++] = wk;
    #else
    queues[queue_id].data[queues[queue_id].size] = wk;
    push_up(queues[queue_id].size);
    queues[queue_id].size++;
    #endif
    ReleaseSRWLockExclusive(&queues[queue_id].queue_lock);
}

struct queued_worker *queue_extract(int64_t threadId)
{
    AcquireSRWLockExclusive(&queues[threadId].queue_lock);
    if (queues[threadId].size != 0)
    {
        queue_size--;
        #ifdef USE_ARRAY
        void *res = queues[threadId].data[--queues[threadId].size];
        #else
        void *res = queues[threadId].data[0];
        queues[threadId].size--;
        queues[threadId].data[0] = queues[threadId].data[queues[threadId].size];
        push_down(0);
        #endif
        ReleaseSRWLockExclusive(&queues[threadId].queue_lock);
        return res;
    }
    ReleaseSRWLockExclusive(&queues[threadId].queue_lock);
    
    // use thread queues[SHARED_QUEUE], if it is empty - use shared queues[SHARED_QUEUE]
    
    AcquireSRWLockExclusive(&queues[SHARED_QUEUE].queue_lock);
    if (queues[SHARED_QUEUE].size == 0)
    {
        ReleaseSRWLockExclusive(&queues[SHARED_QUEUE].queue_lock);
        return NULL;
    }
    queue_size--;
    #ifdef USE_ARRAY
    void *res = queues[SHARED_QUEUE].data[--queues[SHARED_QUEUE].size];
    #else
    void *res = queues[SHARED_QUEUE].data[0];
    queues[SHARED_QUEUE].size--;
    queues[SHARED_QUEUE].data[0] = queues[SHARED_QUEUE].data[queues[SHARED_QUEUE].size];
    push_down(0);
    #endif
    ReleaseSRWLockExclusive(&queues[SHARED_QUEUE].queue_lock);
    return res;
}
