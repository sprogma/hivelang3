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
struct hashtable local_objects;
struct hashtable query_requests;
struct hashtable push_requests;
struct hashtable known_hives;

int64_t equal_bytes(BYTE *a, BYTE *b, int64_t len)
{
    while (len-- && *a++ == *b++);
    return len == -1;
}


static uint64_t GetKnownHiveHash(BYTE *address, int64_t address_length)
{
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

void InitInternalStructures()
{
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
    INIT_HASHTABLE(local_objects);
    INIT_HASHTABLE(known_objects);
    INIT_HASHTABLE(query_requests);
    INIT_HASHTABLE(push_requests);
    INIT_HASHTABLE(known_hives);
}
