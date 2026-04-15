#include "system.h"
#include "immintrin.h"

#include "runtime_lib.h"
#include "runtime.h"
#include "stdatomic.h"

#define RING_SIZE (64 * 1024)
#define RING_MASK (RING_SIZE - 1)

struct task_queue 
{
    _Atomic(void*) buffer[RING_SIZE];
    _Atomic int64_t head;
    _Atomic int64_t tail;
};

static void queue_init(struct task_queue *q) 
{
    memset(q->buffer, 0, sizeof(q->buffer));
    q->head = q->tail = 0;
}

static void queue_push(struct task_queue *q, void *data) 
{
    int64_t t = atomic_fetch_add_explicit(&q->tail, 1, memory_order_relaxed);
    atomic_store_explicit(&q->buffer[t & RING_MASK], data, memory_order_release);
}

static void* queue_pop(struct task_queue *q) 
{
    int64_t h = atomic_load_explicit(&q->head, memory_order_relaxed);

    while (1) 
    {
        int64_t t = atomic_load_explicit(&q->tail, memory_order_acquire);
        if (h >= t) // empty
        {
            return NULL;
        }
        // take next cell
        if (atomic_compare_exchange_weak_explicit(&q->head, &h, h + 1, 
                                                 memory_order_relaxed, 
                                                 memory_order_relaxed)) 
        {
            // wait for data
            void *data;
            while ((data = atomic_load_explicit(&q->buffer[h & RING_MASK], memory_order_acquire)) == NULL) 
            {
                _mm_pause();
            }
            
            // clear data
            atomic_store_explicit(&q->buffer[h & RING_MASK], NULL, memory_order_release);
            return data;
        }
    }
}


struct scheduler_queue
{
    struct task_queue queues[64];
    _Atomic uint64_t active_mask;
    _Atomic int64_t size;
};

static void scheduler_queue_init(struct scheduler_queue *s) 
{
    for (int i = 0; i < 64; i++) 
    {
        queue_init(&s->queues[i]);
    }
    s->active_mask = s->size, 0;
}

static void scheduler_queue_push(struct scheduler_queue *s, int priority, void *data) 
{
    assert(priority >= 0 && priority <= 64);
    assert(data);

    queue_push(&s->queues[priority], data);
    
    atomic_fetch_or_explicit(&s->active_mask, (1ULL << (uint64_t)priority), memory_order_release);
    atomic_fetch_add_explicit(&s->size, 1, memory_order_relaxed);
}

static void* scheduler_queue_pop(struct scheduler_queue *s) 
{
    uint64_t mask = atomic_load_explicit(&s->active_mask, memory_order_acquire);

    while (mask != 0) 
    {
        int priority = (mask == 0 ? 0 : 63 - __builtin_clzll(mask));

        void *data = queue_pop(&s->queues[priority]);
        if (data) 
        {
            atomic_fetch_sub_explicit(&s->size, 1, memory_order_relaxed);
            return data;
        }

        // this is empty bucket - clear it
        atomic_fetch_and_explicit(&s->active_mask, ~(1ULL << (uint64_t)priority), memory_order_release);
        mask = atomic_load_explicit(&s->active_mask, memory_order_acquire);
    }

    return NULL;
}

static inline uint64_t scheduler_rnd(uint64_t *state) 
{
    uint64_t x = *state;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    return *state = x;
}

void scheduler_init(struct scheduler *s, size_t workers_len)
{
    s->len = 0;
    s->workers_len = workers_len;
    s->rnd_state = GetTicks();
    s->workers = myMalloc(sizeof(*s->workers) * s->workers_len);

    for (size_t i = 0; i < s->workers_len; ++i)
    {
        scheduler_queue_init(&s->workers[i]);
    }
}

void scheduler_enqueue_with_affinity(struct scheduler *s, size_t affinity, int priority, struct queued_worker *wk) 
{
    s->len++;
    scheduler_queue_push(&s->workers[affinity % s->workers_len], priority, wk);
}

void scheduler_enqueue(struct scheduler *s, int priority, struct queued_worker *wk) 
{
    if (Workers[wk->id].affinity != -1)
    {
        scheduler_enqueue_with_affinity(s, Workers[wk->id].affinity, priority, wk);
        return;
    }
    size_t i = (size_t)scheduler_rnd(&s->rnd_state) % s->workers_len;
    size_t j = (size_t)scheduler_rnd(&s->rnd_state) % s->workers_len;

    int64_t si = atomic_load_explicit(&s->workers[i].size, memory_order_relaxed);
    int64_t sj = atomic_load_explicit(&s->workers[j].size, memory_order_relaxed);

    size_t target = (si < sj) ? i : j;
    scheduler_queue_push(&s->workers[target], priority, wk);
    s->len++;
}

struct queued_worker *scheduler_dequeue(struct scheduler *s, size_t worker_id) 
{
    void *data = scheduler_queue_pop(&s->workers[worker_id]);
    if (data) 
    {
        s->len--;
        return data;
    }

    // steal data from another worker
    size_t victim_id = worker_id;
    for (size_t n = 0; n < s->workers_len; n++) 
    {
        victim_id = (victim_id + 1 >= s->workers_len ? 0 : victim_id + 1);
        
        if (atomic_load_explicit(&s->workers[victim_id].active_mask, memory_order_acquire) != 0) 
        {
            data = scheduler_queue_pop(&s->workers[victim_id]);
            if (data) 
            {
                s->len--;
                return data;
            }
        }
    }
    return NULL;
}

struct scheduler glb_scheduler;

