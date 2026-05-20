#include "system.h"
#include "immintrin.h"

#include "runtime_lib.h"
#include "remote.h"


lock_t connections_lock = INIT_LOCK;
struct hive_connection *connections[1024];
int64_t connections_len = 0;

lock_t pages_lock = INIT_LOCK;
struct memory_page pages[128];
int64_t pages_len = 0;


struct hashtable * _Atomic get_wait_list;
struct hashtable * _Atomic set_wait_list;

struct i64hashtable * _Atomic get_id_ids;
struct hashtable * _Atomic get_id_broadcasts;

struct i64hashtable * _Atomic get_page_ids;
struct hashtable * _Atomic get_page_broadcasts;

struct hashtable * _Atomic get_path_broadcasts;
struct hashtable * _Atomic get_path_to_id_broadcasts;

struct i64hashtable * _Atomic local_objects;

struct i64hashtable * _Atomic object_paths;
struct i64hashtable * _Atomic global_id_paths;

/// ------------------------------------------------------------- open hash table


int64_t equal_bytes(BYTE *a, BYTE *b, int64_t len)
{
    if (len == 8)
    {
        return *(int64_t *)a == *(int64_t *)b;
    }
    while (len >= 8 && *(int64_t *)a == *(int64_t *)b)
    {
        len -= 8;
    }
    while (len-- && *a++ == *b++);
    return len == -1;
}


uint64_t GetByteStringHash(const void *address, int64_t address_length)
{
    uint64_t hash = 123;
    int64_t i = 0;
    while (i < address_length)
    {
        hash = (hash * 37 + ((BYTE *)address)[i++]);
    } 
    return hash == 0 ? 1 : 0;
}


/// ------------------------------------------------------------------------ open hash

static inline uint64_t GetInt64Hash(int64_t key)
{
    key += 1ULL;
    key ^= key >> 33ULL;
    key *= 0xff51afd7ed558ccdULL;
    key ^= key >> 33ULL;
    key *= 0xc4ceb9fe1a85ec53ULL;
    key ^= key >> 33ULL;
    return key;
}

static inline int64_t i64_raw_update(struct i64hashtable *h, int64_t key, int64_t new_value, int64_t old_value)
{    
    int64_t alloc = h->alloc;
    uint64_t hash = GetInt64Hash(key) % alloc;

#ifdef __CX16__
    union i64hashtable_node_128 new_node = {{key, new_value}};
#endif

    while (1)
    {
        uint64_t idx = hash;
        
#ifdef __CX16__
        unsigned __int128 expected = 0; // empty key == 0, empty value == 0
        union i64hashtable_node_128 *node = (void *)&h->table[idx];

        if (old_value == 0 && atomic_compare_exchange_strong(&node->t, &expected, new_node.t))
        {
            atomic_fetch_add(&h->len, 1);
            return 0;
        }
        else
        {
            if (old_value != 0) expected = atomic_load(&node->t);
            union i64hashtable_node_128 *actual = (union i64hashtable_node_128 *)&expected;
            if (actual->key == key)
            {                
                if (actual->value != old_value) return actual->value;
                if (atomic_compare_exchange_strong(&node->t, &expected, new_node.t))
                {
                    return old_value;
                }
                return actual->value;
            }
        }
#else
        struct i64hashtable_node *node = &h->table[idx];

        int64_t expected = STATE_FREE;
        if (atomic_compare_exchange_strong(&node->state, &expected, STATE_BUSY)) 
        {
            atomic_store(&node->key, key);
            atomic_store(&node->value, new_value);
            atomic_store(&node->state, STATE_READY);
            atomic_fetch_add(&h->len, 1);
            return 0;
        }
        else
        {
            if (expected == STATE_BUSY)
            {
                while (atomic_load(&node->state) == STATE_BUSY)
                {
                    _mm_pause();
                }
            }
            if (atomic_load(&node->key) == key)
            {
                int64_t expected_val = old_value;
                if (atomic_compare_exchange_strong(&node->value, &expected_val, new_value))
                {
                    return old_value;
                }
                return expected_val;
            }
        }
#endif

        hash = (hash + 1 == (uint64_t)alloc ? 0 : hash + 1);
    }
    return 0;
}

int64_t i64_get(struct i64hashtable *h, int64_t key)
{
    int64_t alloc = h->alloc;
    uint64_t hash = GetInt64Hash(key) % alloc;
    while (1)
    {
#ifdef __CX16__
        union i64hashtable_node_128 *node = (void *)&h->table[hash];
        union i64hashtable_node_128 node_value = {.t = atomic_load(&node->t)};
        if (node_value.key == 0) break;
        if (node_value.key == key) 
        {
            return node_value.value;
        }

#else
        struct i64hashtable_node *node = &h->table[hash];
        int64_t state = atomic_load(&node->state);

        if (state == STATE_FREE) break;
        
        while (atomic_load(&node->state) == STATE_BUSY)
        {
            _mm_pause();
        }

        if (atomic_load(&node->key) == key) 
        {
            return atomic_load(&node->value);
        }
#endif

        hash = ((int64_t)(hash + 1) == alloc ? 0 : hash + 1);
    }

    if (h->prev) 
    {
        int64_t val = i64_get(h->prev, key);
        if (val != 0) 
        {
            int64_t res = i64_raw_update(h, key, val, 0);
            if (res != 0) return res; // if there already was some value, return it.
        }
        return val;
    }

    return 0;
}

int64_t i64GetHashtable(struct i64hashtable * _Atomic *ph, int64_t key)
{
    return i64_get(atomic_load(ph), key);
}

int64_t i64SetHashtable(struct i64hashtable * _Atomic *ph, int64_t key, int64_t new_value, int64_t old_value)
{    
    struct i64hashtable *h = atomic_load(ph);
    int64_t result = i64_raw_update(h, key, new_value, old_value);
    
    // need 4 becouse data will be copied from prev versions 
    if (result == 0 && old_value == 0 && atomic_load(&h->len) > h->alloc / 4) // result == 0 means it was insert
    {
        int64_t expected = 0;
        if (atomic_compare_exchange_strong(&h->updating, &expected, 1)) 
        {
            struct i64hashtable *nt = myMalloc(sizeof(struct i64hashtable));
            nt->len = 0; // h->len // 0 to not count was/not was.
            nt->updating = 0;
            nt->alloc = 2 * h->alloc;
            nt->table = myMalloc(sizeof(*nt->table) * nt->alloc);
            memset(nt->table, 0, sizeof(*nt->table) * nt->alloc);
            nt->prev = h;
            
            atomic_store(ph, nt);
        }
    }

    return result;
}



/// ---------------------------------------------------------------------- BYTE LOCK FREE!!!!

static inline void* hashtable_alloc_key_space(struct hashtable *h)
{
    int64_t key_size = h->key_size;
    int64_t seg_malloc_size = SEGMENT_CAPACITY * key_size;
    while (1)
    {
        int64_t local_offset = atomic_fetch_add(&h->offset, 1);
        int64_t local_idx = local_offset % SEGMENT_CAPACITY;
        BYTE *seg = atomic_load(&h->current_segment);
        
        if (!seg || local_idx == 0)
        {
            int64_t alloc_expected = 0;
            if (atomic_compare_exchange_strong(&h->segment_allocating, &alloc_expected, 1))
            {
                seg = atomic_load(&h->current_segment);
                
                if (!seg || local_offset >= (int64_t)(local_offset / SEGMENT_CAPACITY) * SEGMENT_CAPACITY)
                {
                    BYTE *new_seg = myMalloc(seg_malloc_size);
                    atomic_store(&h->current_segment, new_seg);
                    seg = new_seg;
                }
                atomic_store(&h->segment_allocating, 0);
            }
            else
            {
                while (1)
                {
                    BYTE *next_seg = atomic_load(&h->current_segment);
                    if (next_seg != seg)
                    {
                        seg = next_seg;
                        break;
                    }
                    _mm_pause();
                }
            }
        }
        return &seg[local_idx * key_size];
    }
}



static inline int64_t hashtable_raw_update(struct hashtable *h, const void *key, int64_t new_value, int64_t old_value)
{    
    int64_t alloc = h->alloc;
    int64_t key_len = h->key_size;
    uint64_t hash = GetByteStringHash(key, key_len) % alloc;

#ifdef __CX16__
    // first, find existing cell in hashtable
    while (1)
    {
        uint64_t idx = hash;
        union hashtable_node_128 *node = (void *)&h->table[idx];
        unsigned __int128 expected = atomic_load(&node->t);
        union hashtable_node_128 *actual = (union hashtable_node_128 *)&expected;

        if (actual->key_ptr == NULL) break;

        if (memcmp(actual->key_ptr, key, key_len) == 0)
        {
            if (actual->value != old_value) return actual->value;
            
            union hashtable_node_128 update_node = {{actual->key_ptr, new_value}};
            while (!atomic_compare_exchange_strong(&node->t, &expected, update_node.t))
            {
                assert(actual->key_ptr != NULL && memcmp(actual->key_ptr, key, key_len) == 0);
                if (actual->value != old_value) return actual->value;
            }
            return old_value;
        }
        hash = (hash + 1 == (uint64_t)alloc ? 0 : hash + 1);
    }

    // now, try to create new item this this key

    assert(old_value == 0); // now, item must be new to this hashtable
    
    void *allocated_key = hashtable_alloc_key_space(h);
    memcpy(allocated_key, key, key_len);
    union hashtable_node_128 new_node = {{allocated_key, new_value}};

    while (1)
    {
        uint64_t idx = hash;
        unsigned __int128 expected = 0;
        union hashtable_node_128 *node = (void *)&h->table[idx];

        if (atomic_compare_exchange_strong(&node->t, &expected, new_node.t))
        {
            atomic_fetch_add(&h->len, 1);
            return 0;
        }
        else
        {
            union hashtable_node_128 *actual = (union hashtable_node_128 *)&expected;
            if (actual->key_ptr != NULL && memcmp(actual->key_ptr, key, key_len) == 0)
            {
                if (actual->value != old_value) return actual->value;
                union hashtable_node_128 update_node = {{actual->key_ptr, new_value}};
                if (atomic_compare_exchange_strong(&node->t, &expected, update_node.t))
                {
                    return old_value;
                }
                return actual->value;
            }
        }
        hash = (hash + 1 == (uint64_t)alloc ? 0 : hash + 1);
    }
#else
    while (1)
    {
        uint64_t idx = hash;
        struct hashtable_node *node = &h->table[idx];
        int64_t expected = STATE_FREE;

        if (old_value == 0 && atomic_compare_exchange_strong(&node->state, &expected, STATE_BUSY)) 
        {
            void *allocated_key = hashtable_alloc_key_space(h);
            memcpy(allocated_key, key, key_len);

            atomic_store(&node->key_ptr, allocated_key);
            atomic_store(&node->value, new_value);
            atomic_store(&node->state, STATE_READY);
            atomic_fetch_add(&h->len, 1);
            return 0;
        }
        else
        {
            if (expected == STATE_BUSY)
            {
                while (atomic_load(&node->state) == STATE_BUSY) _mm_pause();
            }
            void *actual_key_ptr = atomic_load(&node->key_ptr);
            if (actual_key_ptr != NULL && memcmp(actual_key_ptr, key, key_len) == 0)
            {
                int64_t expected_val = old_value;
                if (atomic_compare_exchange_strong(&node->value, &expected_val, new_value))
                {
                    return old_value;
                }
                return expected_val;
            }
        }
        hash = (hash + 1 == (uint64_t)alloc ? 0 : hash + 1);
    }
#endif
    return 0;
}

static int64_t hashtable_get(struct hashtable *h, const void *key);

static inline int64_t hashtable_take_tagged(struct hashtable *h, const void *key, int64_t mask, int64_t compareto)
{    
    int64_t alloc = h->alloc;
    int64_t key_len = h->key_size;
    uint64_t hash = GetByteStringHash(key, key_len) % alloc;

#ifdef __CX16__
    // first, find existing cell in hashtable
    while (1)
    {
        uint64_t idx = hash;
        union hashtable_node_128 *node = (void *)&h->table[idx];
        unsigned __int128 expected = atomic_load(&node->t);
        union hashtable_node_128 *actual = (union hashtable_node_128 *)&expected;

        if (actual->key_ptr == NULL) break;

        if (memcmp(actual->key_ptr, key, key_len) == 0)
        {
            // want to swap X with X+1 if X < compareto, so use:
            while (1)
            {
                if ((actual->value & mask) >= compareto) // wait while tag will be ok
                {
                    _mm_pause();
                    expected = atomic_load(&node->t);
                    continue;
                }
                int64_t res_val = actual->value & (~mask);
                if (res_val == 0) return 0; // if there is NULL, don't increment tag
                union hashtable_node_128 update_node = {{actual->key_ptr, (actual->value & mask) + 1}}; // set to NULL pointer
                if (atomic_compare_exchange_strong(&node->t, &expected, update_node.t))
                {
                    return res_val;
                }
            }
        }
        hash = (hash + 1 == (uint64_t)alloc ? 0 : hash + 1);
    }
#else
    while (1)
    {
        struct hashtable_node *node = &h->table[hash];
        int64_t state = atomic_load(&node->state);

        if (state == STATE_FREE) break;
        
        while (atomic_load(&node->state) == STATE_BUSY)
        {
            _mm_pause();
        }

        void *actual_key_ptr = atomic_load(&node->key_ptr);
        if (actual_key_ptr != NULL && memcmp(actual_key_ptr, key, key_len) == 0)
        {
            int64_t expected_val = atomic_load(&node->value), new_value;
            while (1)
            {
                if ((expected_val & mask) >= compareto) // wait while tag will be ok
                {
                    _mm_pause();
                    expected_val = atomic_load(&node->value);
                    continue;
                }
                int64_t res_val = expected_val & (~mask);
                if (res_val == 0) return 0; // if there is NULL, don't increment tag
                new_value = (expected_val & mask) + 1; // set to NULL pointer
                if (atomic_compare_exchange_strong(&node->value, &expected_val, new_value))
                {
                    return res_val;
                }                    
            }
        }
        
        hash = (hash + 1 == (uint64_t)alloc ? 0 : hash + 1);
    }
#endif


    if (h->prev) 
    {
        // copy to this layer?
        int64_t val = hashtable_get(h->prev, key);
        if (val != 0)
        {
            int64_t res = hashtable_raw_update(h, key, val, 0);
            if (res != 0)
            {
                // was copied by someone else - ok
            }
            // repeat call, now, key will be 100% found
            return hashtable_take_tagged(h, key, mask, compareto);
        }
        return 0;
    }

    return 0;
}


static int64_t hashtable_get(struct hashtable *h, const void *key)
{
    int64_t alloc = h->alloc;
    int64_t key_len = h->key_size;
    uint64_t hash = GetByteStringHash(key, key_len) % alloc;

    while (1)
    {
#ifdef __CX16__
        union hashtable_node_128 *node = (void *)&h->table[hash];
        union hashtable_node_128 node_value = {.t = atomic_load(&node->t)};
        
        if (node_value.key_ptr == NULL) break;
        
        if (memcmp(node_value.key_ptr, key, key_len) == 0) 
        {
            return node_value.value;
        }
#else
        struct hashtable_node *node = &h->table[hash];
        int64_t state = atomic_load(&node->state);

        if (state == STATE_FREE) break;
        
        while (atomic_load(&node->state) == STATE_BUSY)
        {
            _mm_pause();
        }

        void *actual_key_ptr = atomic_load(&node->key_ptr);
        if (actual_key_ptr != NULL && memcmp(actual_key_ptr, key, key_len) == 0) 
        {
            return atomic_load(&node->value);
        }
#endif

        hash = ((int64_t)(hash + 1) == alloc ? 0 : hash + 1);
    }

    if (h->prev) 
    {
        int64_t val = hashtable_get(h->prev, key);
        if (val != 0) 
        {
            int64_t res = hashtable_raw_update(h, key, val, 0);
            if (res != 0) return res; 
        }
        return val;
    }

    return 0;
}



int64_t hashtable_add(struct hashtable *h, const void *key, int64_t delta)
{
    int64_t alloc = h->alloc;
    int64_t key_len = h->key_size;
    uint64_t hash = GetByteStringHash(key, key_len) % alloc;

    while (1)
    {
#ifdef __CX16__
        union hashtable_node_128 *node = (void *)&h->table[hash];
        union hashtable_node_128 node_value = {.t = atomic_load(&node->t)};
        
        if (node_value.key_ptr == NULL) break;
        
        if (memcmp(node_value.key_ptr, key, key_len) == 0) 
        {
            unsigned __int128 old_value = node_value.t;
            do 
            {
                node_value.t = old_value;
                node_value.value += delta;
            } 
            while (!atomic_compare_exchange_weak(&node->t, &old_value, node_value.t));

            return node_value.value;
        }
#else
        struct hashtable_node *node = &h->table[hash];
        int64_t state = atomic_load(&node->state);

        if (state == STATE_FREE) break;
        
        while (atomic_load(&node->state) == STATE_BUSY)
        {
            _mm_pause();
        }

        void *actual_key_ptr = atomic_load(&node->key_ptr);
        if (actual_key_ptr != NULL && memcmp(actual_key_ptr, key, key_len) == 0) 
        {
            return atomic_fetch_add(&node->value, delta);
        }
#endif

        hash = ((int64_t)(hash + 1) == alloc ? 0 : hash + 1);
    }

    if (h->prev) 
    {
        int64_t val = hashtable_get(h->prev, key);
        if (val != 0) 
        {
            int64_t res = hashtable_raw_update(h, key, val + delta, 0);
            if (res != 0)
            {
                return hashtable_add(h, key, delta);
            }
            return val + delta;
        }
        return 0;
    }

    return 0;
}



int64_t GetHashtable(struct hashtable * _Atomic *ph, void *key)
{
    return hashtable_get(atomic_load(ph), key);
}


int64_t TakeTaggedHashtable(struct hashtable * _Atomic *ph, void *key, int64_t mask, int64_t compareto)
{
    return hashtable_take_tagged(atomic_load(ph), key, mask, compareto);
}


int64_t AddHashtable(struct hashtable * _Atomic *ph, void *key, int64_t delta)
{
    return hashtable_add(atomic_load(ph), key, delta);
}


int64_t SetHashtable(struct hashtable * _Atomic *ph, void *key, int64_t new_value, int64_t old_value)
{
    struct hashtable *h = atomic_load(ph);
    int64_t result = hashtable_raw_update(h, key, new_value, old_value);
    
    if (result == 0 && old_value == 0 && atomic_load(&h->len) > h->alloc / 4)
    {
        int64_t expected = 0;
        if (atomic_compare_exchange_strong(&h->updating, &expected, 1)) 
        {
            struct hashtable *nt = myMalloc(sizeof(struct hashtable));
            nt->len = 0; 
            nt->updating = 0;
            nt->alloc = 2 * h->alloc;
            nt->prev = h;

            nt->key_size = h->key_size;

            nt->offset = 0;
            nt->current_segment = NULL;
            nt->segment_allocating = 0;

            nt->table = myMalloc(sizeof(*nt->table) * nt->alloc);
            memset(nt->table, 0, sizeof(*nt->table) * nt->alloc);
            
            atomic_store(ph, nt);
        }
    }

    return result;
}

/// ---------------------------------------------------------------------- initialization

void InitInternalStructures()
{
    #define INIT_HASHTABLE64(h) \
        h = myMalloc(sizeof(*h)); \
        h->len = 0; \
        h->updating = 0; \
        h->prev = NULL; \
        h->alloc = 1024; \
        h->table = myMalloc(sizeof(*h->table) * h->alloc); \
        memset(h->table, 0, sizeof(*h->table) * h->alloc);
        
    #define INIT_HASHTABLE_AUTO(h) \
        INIT_HASHTABLE(h, sizeof(struct h ## _key))
        
    #define INIT_HASHTABLE_BROADCAST(h) \
        INIT_HASHTABLE(h, BROADCAST_ID_LENGTH)
        
    #define INIT_HASHTABLE(h, k_size) \
        h = myMalloc(sizeof(*h)); \
        h->len = 0; \
        h->updating = 0; \
        h->prev = NULL; \
        h->alloc = 1024; \
        h->key_size = (k_size); \
        h->offset = 0; \
        h->current_segment = NULL; \
        h->segment_allocating = 0; \
        h->table = myMalloc(sizeof(*h->table) * h->alloc); \
        memset(h->table, 0, sizeof(*h->table) * h->alloc);

    INIT_HASHTABLE_AUTO(get_wait_list);
    INIT_HASHTABLE_AUTO(set_wait_list);

    INIT_HASHTABLE64(get_id_ids);
    INIT_HASHTABLE_BROADCAST(get_id_broadcasts);

    INIT_HASHTABLE64(get_page_ids);
    INIT_HASHTABLE_BROADCAST(get_page_broadcasts);

    INIT_HASHTABLE_BROADCAST(get_path_broadcasts);
    INIT_HASHTABLE_BROADCAST(get_path_to_id_broadcasts);

    INIT_HASHTABLE64(local_objects);

    INIT_HASHTABLE64(object_paths);
    INIT_HASHTABLE64(global_id_paths);
}



