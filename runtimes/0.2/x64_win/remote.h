#ifndef REMOTE_H
#define REMOTE_H

#ifndef _WIN32_WINNT
    #define _WIN32_WINNT _WIN32_WINNT_VISTA
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
#include "windows.h"

#include "inttypes.h"
#include "stdatomic.h"
#include "runtime_lib.h"


// --------------------------------- network api -------------------------------

struct connection_context
{
    OVERLAPPED overlapped;
    SOCKET socket;
    WSABUF wsaBuf;
    struct hive_connection *connection;

    // to store current data - callback is called when buffer_len is received
    int64_t res_api_call;
    int64_t res_buffer_len;
    int64_t res_buffer_current_len;
    BYTE *res_buffer;
    BYTE buffer[4096];
};

struct hive_connection
{
    struct connection_context *ctx;
    SOCKET outgoing;
    int64_t local_id;
    // temporary fields, use them only then connecting (becouse address can change)
    SOCKADDR_STORAGE address;
    int address_len;
};

#define OBJECTS_PER_PAGE (1<<(8*3))

struct memory_page
{
    int64_t id;
    int64_t object_count;
    _Atomic int64_t next_allocated_id;
};

struct memory_page_request
{
    int64_t page_id;
    int64_t local_redirect_id;
    _Atomic int32_t answered;
    int32_t requested;
};

struct hashtable_node
{
    struct hashtable_node *next;
    int64_t id;
    int64_t length;
    BYTE bytes[];
};


struct hashtable
{
    SRWLOCK lock;
    struct hashtable_node **table;
    int64_t len;
    int64_t alloc;
};


// TODO: remove 1024 as constant
extern SRWLOCK connections_lock;
extern struct hive_connection *connections[];
extern int64_t connections_len;

extern SRWLOCK pages_lock;
extern struct memory_page pages[];
extern int64_t pages_len;

extern struct hashtable known_hives;
extern struct hashtable known_broadcasts;


int64_t equal_bytes(BYTE *a, BYTE *b, int64_t len);


int64_t GetHashtable(struct hashtable *h, BYTE *address, int64_t address_length, int64_t default_value);
int64_t GetHashtableNoLock(struct hashtable *h, BYTE *address, int64_t address_length, int64_t default_value);
void SetHashtable(struct hashtable *h, BYTE *address, int64_t address_length, int64_t new_value);
void SetHashtableNoLock(struct hashtable *h, BYTE *address, int64_t address_length, int64_t new_value);

void RequestObjectGet(int64_t object, int64_t offset, int64_t size);




// --------------------------------- hive api -------------------------------

/*
    there is many tables of objects: 
        - known objects table
            this is DNS-like structure which say from which hive you can fastest get to this object
            [object_id -> hive_id]
        - local objects table [implicit]
            this table stores conversion from object_id to void * [to raw object]
        - local object copy table
            // TODO: do we need some cahe for remote objects? may be create it after LOCK call?
        - global objects await queue
            stores all requests, and callbacks to do after they will be received
            [object_id, offset, size -> rules]
            rules may be:
                send answer to local_id=...
                continue worker from ... [as now does]

    answer on QUERY_OBJECT will be produced using this steps:
        if object is found in local table:
            simply send bytes [store bytes]
        if object is found in known objects table [and hive still exists]
            redirect query to that hive
        else 
            send broadcast query to find fastest [any] way to object

    aftert getting answer on QUERY_OBJECT:
        for each rule from await queue:
            if it is continue worker from...
                move worker to ready queue [as now does]
            if it is send to local_id=...
                send result + add ip+port of hive from which we got answer -
                if he have connection to that hive - he will update his known objects table

    table structure:
    
        known objects table:
            hash table

        local objects table:
            any object inside worker is pointer to struct object;
            there located int64_t object_id;

        local object copy table:
            not implemented

        global objects await queue:
            hash table to forward-linked lists
*/

struct known_object
{
    int64_t local_id; // hive to ask
};

struct local_object_table_1
{
    struct local_object_table_2 *lay[256];
};

struct local_object_table_2
{
    struct local_object_table_3 *lay[256];
};

struct local_object_table_3
{
    void *lay[256];
};

extern struct hashtable known_objects;
extern struct hashtable local_objects;


// ------------- other -----------

void InitInternalStructures();
void start_remote_subsystem();


#endif
