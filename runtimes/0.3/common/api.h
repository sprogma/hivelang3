#ifndef API_H
#define API_H

#include "stdatomic.h"
#include "inttypes.h"
#include "system.h"

#ifdef __CX16__
    struct i64hashtable_node
    {
        int64_t key;
        int64_t value;
    } __attribute__((aligned(16)));
    
    union i64hashtable_node_128
    {
        struct i64hashtable_node;
        _Atomic unsigned __int128 t;
    };
#else
    #define STATE_FREE 0
    #define STATE_BUSY 1
    #define STATE_READY 2

    struct i64hashtable_node
    {
        _Atomic int64_t key;
        _Atomic int64_t value;
        _Atomic int64_t state;
    };
#endif

struct i64hashtable
{
    _Atomic int64_t updating;
    struct i64hashtable * _Atomic prev;
    struct i64hashtable_node *table;
    _Atomic int64_t len;
    int64_t alloc;
};

#define SEGMENT_CAPACITY 4096

#ifdef __CX16__
    struct hashtable_node
    {
        void *key_ptr;
        int64_t value;
    } __attribute__((aligned(16)));
    
    union hashtable_node_128
    {
        struct hashtable_node;
        _Atomic unsigned __int128 t;
    };
#else
    #define STATE_FREE 0
    #define STATE_BUSY 1
    #define STATE_READY 2
    
    struct hashtable_node
    {
        _Atomic (void *) key_ptr;
        _Atomic int64_t value;
        _Atomic int64_t state;
    };
#endif

struct hashtable
{
    _Atomic int64_t updating;
    struct hashtable * _Atomic prev;
    struct hashtable_node *table;
    _Atomic int64_t len;
    int64_t alloc;
    
    int64_t key_size;
    _Atomic int64_t offset;
    BYTE * _Atomic current_segment; 
    _Atomic int64_t segment_allocating;
};


int64_t equal_bytes(BYTE *a, BYTE *b, int64_t len);

uint64_t GetByteStringHash(const void *address, int64_t address_length);

int64_t GetHashtable(struct hashtable * _Atomic *ph, void *key);
int64_t TakeTaggedHashtable(struct hashtable * _Atomic *ph, void *key, int64_t mask, int64_t compareto);
int64_t AddHashtable(struct hashtable * _Atomic *ph, void *key, int64_t delta);
int64_t SetHashtable(struct hashtable * _Atomic *ph, void *key, int64_t new_value, int64_t old_value);

int64_t i64GetHashtable(struct i64hashtable * _Atomic *h, int64_t key);
int64_t i64SetHashtable(struct i64hashtable * _Atomic *h, int64_t key, int64_t new_value, int64_t old_value);


#endif
