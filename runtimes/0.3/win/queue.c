#define UNICODE 1
#define _UNICODE 1
#define _WIN32_WINNT _WIN32_WINNT_VISTA

#include <winsock2.h>
#include <ws2tcpip.h>
#include "windows.h"

#include "runtime_lib.h"
#include "runtime.h"


// #define USE_ARRAY


struct queue_t queue;


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
    void **a = queue.data;
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
    void **a = queue.data;
    int64_t n = queue.size;
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
    queue.size = 0;
    queue.alloc = 1024*16;
    queue.data = myMalloc(sizeof(*queue.data) * queue.alloc);
    queue.queue_lock = (SRWLOCK)SRWLOCK_INIT;
}

void queue_enqueue(struct queued_worker *wk)
{
    AcquireSRWLockExclusive(&queue.queue_lock);
    #ifdef USE_ARRAY
    queue.data[queue.size++] = wk;
    #else
    queue.data[queue.size] = wk;
    push_up(queue.size);
    queue.size++;
    #endif
    ReleaseSRWLockExclusive(&queue.queue_lock);
}

struct queued_worker *queue_extract()
{
    AcquireSRWLockExclusive(&queue.queue_lock);
    if (queue.size == 0)
    {
        ReleaseSRWLockExclusive(&queue.queue_lock);
        return NULL;
    }
    #ifdef USE_ARRAY
    void *res = queue.data[--queue.size];
    #else
    void *res = queue.data[0];
    queue.size--;
    queue.data[0] = queue.data[queue.size];
    push_down(0);
    #endif
    ReleaseSRWLockExclusive(&queue.queue_lock);
    return res;
}
