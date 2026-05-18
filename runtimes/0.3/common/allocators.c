#include "runtime.h"


static void* g_waiting_workers = NULL;
static _Atomic int32_t g_waiting_workers_spinlock = 0;


struct waiting_worker *AllocateWaitingWorker(void)
{
    struct waiting_worker *t = NULL;
    int32_t expect = 0;

    while (!atomic_compare_exchange_weak_explicit(&g_waiting_workers_spinlock, &expect, 1, memory_order_acquire, memory_order_relaxed)) {
        _mm_pause();
        expect = 0;
    }

    if (g_waiting_workers != NULL) {
        t = g_waiting_workers;
        g_waiting_workers = *(void **)g_waiting_workers;
        atomic_store_explicit(&g_waiting_workers_spinlock, 0, memory_order_release);
    } else {
        atomic_store_explicit(&g_waiting_workers_spinlock, 0, memory_order_release);
        t = myMalloc(sizeof(*t));
    }

    return t;
}

void FreeWaitingWorkerBase(struct waiting_worker *t)
{
    if (!t) return;
    int32_t expect = 0;

    while (!atomic_compare_exchange_weak_explicit(&g_waiting_workers_spinlock, &expect, 1, memory_order_acquire, memory_order_relaxed)) {
        _mm_pause();
        expect = 0;
    }

    *(void **)t = g_waiting_workers;
    g_waiting_workers = t;
    atomic_store_explicit(&g_waiting_workers_spinlock, 0, memory_order_release);
}


static void* g_free_workers = NULL;
static _Atomic int32_t g_workers_spinlock = 0;

struct queued_worker *AllocateQueuedWorker(void)
{
    struct queued_worker *t = NULL;
    int32_t expect = 0;

    while (!atomic_compare_exchange_weak_explicit(&g_workers_spinlock, &expect, 1, memory_order_acquire, memory_order_relaxed)) {
        _mm_pause();
        expect = 0;
    }

    if (g_free_workers != NULL) {
        t = g_free_workers;
        g_free_workers = *(void **)g_free_workers;
        atomic_store_explicit(&g_workers_spinlock, 0, memory_order_release);
    } else {
        atomic_store_explicit(&g_workers_spinlock, 0, memory_order_release);
        t = myMalloc(sizeof(*t));
    }

    return t;
}

void FreeQueuedWorker(struct queued_worker *t)
{
    if (!t) return;
    int32_t expect = 0;

    while (!atomic_compare_exchange_weak_explicit(&g_workers_spinlock, &expect, 1, memory_order_acquire, memory_order_relaxed)) {
        _mm_pause();
        expect = 0;
    }

    *(void **)t = g_free_workers;
    g_free_workers = t;
    atomic_store_explicit(&g_workers_spinlock, 0, memory_order_release);
}



static void* g_free_wait_nodes = NULL;
static _Atomic int32_t g_wait_nodes_spinlock = 0;

struct wait_list_node *WaitListNode(struct wait_list_node *node)
{
    struct wait_list_node *old_head = atomic_load_explicit(&wait_list, memory_order_acquire);
    do 
    {
        node->next = old_head;
    } 
    while (!atomic_compare_exchange_weak(&wait_list, &old_head, node));

    atomic_fetch_add_explicit(&wait_list_len, 1, memory_order_relaxed);

    return node;
}


struct wait_list_node *WaitListWorker(struct waiting_worker *t)
{
    struct wait_list_node *node = NULL;
    int32_t expect = 0;
    while (!atomic_compare_exchange_weak(&g_wait_nodes_spinlock, &expect, 1)) 
    {
        _mm_pause(); expect = 0;
    }
    if (g_free_wait_nodes != NULL) 
    {
        node = g_free_wait_nodes;
        g_free_wait_nodes = *(void **)g_free_wait_nodes;
        atomic_store_explicit(&g_wait_nodes_spinlock, 0, memory_order_release);
    } 
    else 
    {
        atomic_store_explicit(&g_wait_nodes_spinlock, 0, memory_order_release);
        node = myMalloc(sizeof(*node));
    }
    node->worker = t;
    log("Worker add to wait list [id=%lld data=%p]\n", t->id, t->data);
    return WaitListNode(node);
}


void FreeWaitingNode(struct wait_list_node *node)
{
    if (!node) return;
    int32_t expect = 0;
    while (!atomic_compare_exchange_weak(&g_wait_nodes_spinlock, &expect, 1)) 
    {
        _mm_pause(); expect = 0;
    }
    *(void **)node = g_free_wait_nodes;
    g_free_wait_nodes = node;
    atomic_store_explicit(&g_wait_nodes_spinlock, 0, memory_order_release);
}


void FreeWaitingWorker(struct waiting_worker *t)
{
    assert(Providers[Workers[t->id].provider].FreeWaitingWorker != 0);
    Providers[Workers[t->id].provider].FreeWaitingWorker(t);
}


