#define _WIN32_WINNT _WIN32_WINNT_VISTA

#include <winsock2.h>
#include <ws2tcpip.h>
#include "windows.h"

#include "runtime_lib.h"
#include "remote.h"


SRWLOCK connections_lock = SRWLOCK_INIT;
struct hive_connection *connections[1024];
int64_t connections_len = 0;

SRWLOCK pages_lock = SRWLOCK_INIT;
struct memory_page pages[128];
int64_t pages_len = 0;

struct hashtable known_id_broadcasts;
struct hashtable known_page_broadcasts;
struct hashtable known_path_broadcasts;
struct hashtable known_path_id_broadcasts;
struct hashtable known_objects;
struct hashtable query_requests;
struct hashtable push_requests;
struct hashtable known_hives;

struct i64hashtable * _Atomic local_objects;


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


static uint64_t GetKnownHiveHash(BYTE *address, int64_t address_length)
{
    if (address_length == 8)
    {  
        int64_t i = *(int64_t *)address;
        i += 1ULL;
        i ^= i >> 33ULL;
        i *= 0xff51afd7ed558ccdULL;
        i ^= i >> 33ULL;
        i *= 0xc4ceb9fe1a85ec53ULL;
        i ^= i >> 33ULL;
        return i;
    }
    uint64_t hash = 123;
    int64_t i = 0;
    while (i < address_length)
    {
        hash = (hash * 27 + address[i++]);
    } 
    return hash;
}


int64_t GetHashtableNoLock(struct hashtable *h, BYTE *address, int64_t address_length, int64_t default_value)
{
    uint64_t hash = GetKnownHiveHash(address, address_length);

    hash %= h->alloc;

    /* try to get value */
    struct hashtable_node *cur = h->table[hash];
    while (cur != NULL)
    {
        if (address_length == cur->length && equal_bytes(cur->bytes, address, address_length))
        {
            int64_t result = cur->id;
            return result;
        }
        cur = cur->next;
    }
    
    return default_value;
}

void SetHashtableNoLock(struct hashtable *h, BYTE *address, int64_t address_length, int64_t new_value)
{    
    uint64_t hash = GetKnownHiveHash(address, address_length);

    hash %= h->alloc;

    /* try to update existing value */
    struct hashtable_node *cur = h->table[hash];
    while (cur != NULL)
    {
        if (address_length == cur->length && equal_bytes(cur->bytes, address, address_length))
        {
            cur->id = new_value;
            return;
        }
        cur = cur->next;
    }

    h->len++;
    if (h->len > h->alloc / 2)
    {
        struct hashtable_node **old_table = h->table;
        int64_t old_table_alloc = h->alloc;
        h->alloc = (h->alloc == 0 ? 64 : h->alloc * 2);
        h->table = myMalloc(sizeof(*h->table) * h->alloc);
        memset(h->table, 0, sizeof(*h->table) * h->alloc);
        
        /* rehash all table */
        for (int64_t i = 0; i < old_table_alloc; ++i)
        {
            cur = old_table[i];
            while (cur != NULL)
            {
                uint64_t new_hash = GetKnownHiveHash(cur->bytes, cur->length) % h->alloc;
                struct hashtable_node *tmp = cur->next;
                cur->next = h->table[new_hash];
                h->table[new_hash] = cur;
                cur = tmp;
            }
        }

        myFree(old_table);

        hash = GetKnownHiveHash(address, address_length) % h->alloc;
    }
    
    /* insert new item */
    cur = myMalloc(sizeof(*cur) + address_length);
    memcpy(cur->bytes, address, address_length);
    cur->length = address_length;
    cur->id = new_value;
    cur->next = h->table[hash];
    h->table[hash] = cur;
}

int64_t GetHashtable(struct hashtable *h, BYTE *address, int64_t address_length, int64_t default_value)
{
    AcquireSRWLockShared(&h->lock);
    int64_t res = GetHashtableNoLock(h, address, address_length, default_value);
    ReleaseSRWLockShared(&h->lock);
    return res;
}

void SetHashtable(struct hashtable *h, BYTE *address, int64_t address_length, int64_t new_value)
{
    AcquireSRWLockExclusive(&h->lock);
    SetHashtableNoLock(h, address, address_length, new_value);
    ReleaseSRWLockExclusive(&h->lock);
}


/// ------------------------------------------------------------------------ closed hash

#ifdef NOT_USE_I64HASHTABLE
int64_t i64GetHashtableNoLock(struct i64hashtable * _Atomic *h, int64_t key, int64_t default_value)
{
    return GetHashtableNoLock(*h, (BYTE *)&key, 8, default_value);
}

void i64SetHashtableNoLock(struct i64hashtable * _Atomic *h, int64_t key, int64_t new_value)
{    
    return SetHashtableNoLock(*h, (BYTE *)&key, 8, new_value);
}

int64_t i64GetHashtable(struct i64hashtable * _Atomic *h, int64_t key, int64_t default_value)
{
    return GetHashtable(*h, (BYTE *)&key, 8, default_value);
}

void i64SetHashtable(struct i64hashtable * _Atomic *h, int64_t key, int64_t new_value)
{
    SetHashtable(*h, (BYTE *)&key, 8, new_value);
}

#else


static inline uint64_t i64GetKnownHiveHash(int64_t key)
{
    key += 1ULL;
    key ^= key >> 33ULL;
    key *= 0xff51afd7ed558ccdULL;
    key ^= key >> 33ULL;
    key *= 0xc4ceb9fe1a85ec53ULL;
    key ^= key >> 33ULL;
    return key;
}

static inline int64_t i64_raw_update(struct i64hashtable *h, int64_t key, int64_t new_value)
{    
    int64_t alloc = h->alloc;
    uint64_t hash = i64GetKnownHiveHash(key) % alloc;

    while (1)
    {
        uint64_t idx = hash;
        struct i64hashtable_node *node = &h->table[idx];

        int64_t state = atomic_load(&node->state);
        if (state != STATE_FREE) 
        {
            while (atomic_load(&node->state) == STATE_BUSY)
            {
                _mm_pause();
            }
            if (atomic_load(&node->key) == key) 
            {
                atomic_store(&node->value, new_value);
                return 1;
            }
            hash = (hash + 1 == (uint64_t)alloc ? 0 : hash + 1);
            continue;
        }
        int64_t expected = STATE_FREE;
        if (atomic_compare_exchange_strong(&node->state, &expected, STATE_BUSY)) 
        {
            atomic_store(&node->key, key);
            atomic_store(&node->value, new_value);
            atomic_store(&node->state, STATE_READY);
            atomic_fetch_add(&h->len, 1);
            return 1;
        }
        hash = (hash + 1 == (uint64_t)alloc ? 0 : hash + 1);
    }
    return 0;
}

int64_t i64_get(struct i64hashtable *h, int64_t key, int64_t default_value)
{
    int64_t alloc = h->alloc;
    uint64_t hash = i64GetKnownHiveHash(key) % alloc;

    while (1)
    {
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

        hash = ((int64_t)(hash + 1) == alloc ? 0 : hash + 1);
    }

    if (h->prev) 
    {
        int64_t val = i64_get(h->prev, key, default_value);
        if (val != default_value) 
        {
            i64_raw_update(h, key, val);
        }
        return val;
    }

    return default_value;
}

int64_t i64GetHashtableNoLock(struct i64hashtable * _Atomic *ph, int64_t key, int64_t default_value)
{
    return i64_get(atomic_load(ph), key, default_value);
}

void i64SetHashtableNoLock(struct i64hashtable * _Atomic *ph, int64_t key, int64_t new_value)
{    
    struct i64hashtable *h = atomic_load(ph);
    int64_t inserted = i64_raw_update(h, key, new_value);
    
    // need 4 becouse data will be copied from prev versions 
    if (inserted && atomic_load(&h->len) > h->alloc / 4) 
    {
        int64_t expected = 0;
        if (atomic_compare_exchange_strong(&h->updating, &expected, 1)) 
        {
            struct i64hashtable *nt = myMalloc(sizeof(struct i64hashtable));
            nt->len = 0; // h->len // 0 to not count was/not was, etc.
            nt->updating = 0;
            nt->alloc = 2 * h->alloc;
            nt->table = myMalloc(sizeof(*nt->table) * nt->alloc);
            nt->prev = h;
            
            atomic_store(ph, nt);
        }
    }
}

int64_t i64GetHashtable(struct i64hashtable * _Atomic *h, int64_t key, int64_t default_value)
{
    return i64GetHashtableNoLock(h, key, default_value);
}

void i64SetHashtable(struct i64hashtable * _Atomic *h, int64_t key, int64_t new_value)
{
    i64SetHashtableNoLock(h, key, new_value);
}
#endif



/// ---------------------------------------------------------------------- initialization

void InitInternalStructures()
{
    #ifdef NOT_USE_I64HASHTABLE
        #define INIT_HASHTABLE64 INIT_HASHTABLE
    #else
    #define INIT_HASHTABLE64(h) \
        h = myMalloc(sizeof(*h)); \
        h->len = 0; \
        h->updating = 0; \
        h->prev = NULL; \
        h->alloc = 1024; \
        h->table = myMalloc(sizeof(*h->table) * h->alloc); \
        memset(h->table, 0, sizeof(*h->table) * h->alloc);
    #endif
        
    #define INIT_HASHTABLE(h) \
        h.lock = (SRWLOCK)SRWLOCK_INIT; \
        h.len = 0; \
        h.alloc = 1024; \
        h.table = myMalloc(sizeof(*h.table) * h.alloc); \
        memset(h.table, 0, sizeof(*h.table) * h.alloc);

    INIT_HASHTABLE(known_id_broadcasts);
    INIT_HASHTABLE(known_page_broadcasts);
    INIT_HASHTABLE(known_path_broadcasts);
    INIT_HASHTABLE(known_path_id_broadcasts);
    INIT_HASHTABLE(known_objects);
    INIT_HASHTABLE(query_requests);
    INIT_HASHTABLE(push_requests);
    INIT_HASHTABLE(known_hives);
    
    INIT_HASHTABLE64(local_objects);
}
