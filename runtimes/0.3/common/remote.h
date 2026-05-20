#ifndef REMOTE_H
#define REMOTE_H

#include "system.h"

#define BROADCAST_ID_LENGTH 27

#include "inttypes.h"
#include "stdatomic.h"
#include "runtime_lib.h"

struct waiting_worker;
struct wait_list_node;

// --------------------------------- network api -------------------------------

struct connection_context
{
    #ifdef _WIN32
    OVERLAPPED overlapped;
    SOCKET socket;
    WSABUF wsaBuf;
    #else
    int socket;
    size_t buffer_to_recv_len;
    #endif
    struct hive_connection *connection;

    // to store current data - callback is called when buffer_len is received
    int64_t res_api_call;
    int64_t res_buffer_len;
    int64_t res_buffer_current_len;
    BYTE *res_buffer;
    BYTE buffer[4096];
};

struct bufferized_socket
{   
    socket_t sock;
    int64_t buffer_len;
    BYTE buffer[1024];
};

#define INT_INFINITY 999999
struct hive_connection
{
    lock_t lock;
    struct connection_context *ctx;
    struct bufferized_socket outgoing;
    int64_t local_id;
    // temporary fields, use them only then connecting (becouse address can change)
    #ifdef _WIN32
    SOCKADDR_STORAGE address;
    int address_len;
    #else
    struct sockaddr_storage address;
    socklen_t address_len;
    #endif
    // usage data
    int64_t wait_list_len;
    int64_t queue_len;
    int64_t idle_time;
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
    _Atomic int32_t requested;
};


#define QUERY_HASHING_BYTES 24
struct linked_node
{
    int64_t local_id;
    struct linked_node *next;
};
struct query_object_request
{
    int64_t object_id;
    int64_t offset;
    int64_t size;
    struct linked_node * _Atomic local_ids;
    struct linked_node * _Atomic wait_list;
};
#define PUSH_HASHING_BYTES 24
struct push_object_request
{
    int64_t object_id;
    int64_t offset;
    int64_t size;
    struct linked_node * _Atomic local_ids;
    struct linked_node * _Atomic wait_list;
};

struct known_hive
{
    int64_t local_id;
    int64_t distance;
};



#include "api.h"

// -------------------- hashtables declarations ------------------------

//
/// hash tables for callbacks on object set/get
//
#define GETSET_WAIT_LIST_VALUE_PROCESSING_TAG 0xF
// [not more than data aligment - so use 8 bits on tagging and 1 worker can update results at one time]
#define GETSET_WAIT_LIST_PARALLEL_PROCESSING 1

struct get_wait_list_key
{
    uint64_t object_id;
    uint64_t offset;
    uint64_t size;
};
struct get_wait_list_value
{
    struct get_wait_list_value *next; // must be first field
    void *params;
    void (*callback)(int64_t object_id, int64_t offset, int64_t size, BYTE *data, void *params);
};
extern struct hashtable * _Atomic get_wait_list;
void callbackQueryAnswerLocalId(int64_t object_id, int64_t offset, int64_t size, BYTE *data, void *params);
void callbackContinueWorkerFromWaitingQuery(int64_t object_id, int64_t offset, int64_t size, BYTE *data, void *params);

struct set_wait_list_key
{
    uint64_t object_id;
    uint64_t offset;
    uint64_t size;
    uint64_t hash;
};
struct set_wait_list_value
{
    struct set_wait_list_value *next; // must be first field
    void *params;
    void (*callback)(int64_t object_id, int64_t offset, int64_t size, int64_t hash, void *params);
};
extern struct hashtable * _Atomic set_wait_list;
void callbackPushAnswerLocalId(int64_t object_id, int64_t offset, int64_t size, int64_t hash, void *params);
void callbackContinueWorkerFromWaitingPush(int64_t object_id, int64_t offset, int64_t size, int64_t hash, void *params);


// 
/// hash tables used for server's ID selection
//
// in ids table, value is live range (result of SheduleTimeoutFromNow) - if it is outdated, 
// broadcast will be counted as new broadcast / new id
extern struct i64hashtable * _Atomic get_id_ids; // known requested id's.
struct get_id_broadcasts_value
{
    int64_t id;
    int64_t local_redirect_id;
    _Atomic int32_t answered;
    _Atomic int32_t requested;
};
extern struct hashtable * _Atomic get_id_broadcasts; // known broadcasts id's, key is broadcast_id of size BROADCAST_ID_LENGTH

//
/// hash tables used for memory pages allocation
//
extern struct i64hashtable * _Atomic get_page_ids; // known requested id's.
struct get_page_broadcasts_value
{
    int64_t page_id;
    int64_t local_redirect_id;
    _Atomic int32_t answered;
    _Atomic int32_t requested;
};
extern struct hashtable * _Atomic get_page_broadcasts;

//
/// hash tables used for path broadcasts
//
extern struct hashtable * _Atomic get_path_broadcasts;

//
/// hash tables used for path to servers broadcasts
//
extern struct hashtable * _Atomic get_path_to_id_broadcasts;

//
/// local objects hashtable
//
// object_id -> object pointer or NULL if it is remote
extern struct i64hashtable * _Atomic local_objects;

//
/// object paths hashtable
//
// stores path to object based on it's id
#define INFINITY_DISTANCE 999999
struct object_paths_value
{
    _Atomic int64_t local_id;
    _Atomic int64_t distance;
};
extern struct i64hashtable * _Atomic object_paths;

//
/// global ids paths hashtable
//
// stores path to object based on it's id
#define INFINITY_DISTANCE 999999
struct global_id_paths_value
{
    _Atomic int64_t local_id;
    _Atomic int64_t distance;
};
extern struct i64hashtable * _Atomic global_id_paths;



// TODO: remove 1024 as constant
extern lock_t connections_lock;
extern struct hive_connection *connections[];
extern int64_t connections_len;

extern lock_t pages_lock;
extern struct memory_page pages[];
extern int64_t pages_len;

extern struct hashtable * _Atomic known_page_broadcasts;
extern struct hashtable * _Atomic known_path_broadcasts;
extern struct hashtable * _Atomic known_path_id_broadcasts;


void RequestObjectGet(int64_t object, int64_t offset, int64_t size, struct waiting_worker *worker);
void RequestObjectSet(int64_t object_id, int64_t offset, int64_t size, void *data, struct waiting_worker *worker);
void StartNewWorkerRemote(struct hive_connection *con, int64_t worker_id, int64_t global_id, void *inputTable);
void RegisterPushEvent(int64_t object_id, int64_t offset, int64_t size, const void *source);



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

// #define PUSH_REPEAT_TIMEOUT (20*1000)
// #define QUERY_REPEAT_TIMEOUT (20*1000)
#define PUSH_REPEAT_TIMEOUT (1*1000)
#define QUERY_REPEAT_TIMEOUT (1*1000)

// ------------- other -----------

void InitInternalStructures();
void start_remote_subsystem(int64_t noStdin);

void UpdateWaitingQuery(int64_t object_id, int64_t offset, int64_t size, BYTE *data);
void UpdateWaitingPush(int64_t object_id, int64_t offset, int64_t size, int64_t hash);

void GetsetInsertTagged(struct hashtable * _Atomic *table, void *key, void *new_node);

void SendPageAllocationConfirm(struct hive_connection *con, BYTE *broadcast_id);
void SendIDConfirm(struct hive_connection *con, BYTE *broadcast_id);
void ConfirmConnection(struct hive_connection *con, int64_t local_id, int64_t port);
void RedirectBroadcastQuery(int64_t page_id, BYTE *broadcast_id, int64_t except_this_local_id, _Atomic int32_t *send_counter);
void RedirectBroadcastIDQuery(int64_t want_id, BYTE *broadcast_id, int64_t except_this_local_id, _Atomic int32_t *send_counter);
void RequestObjectPathBroadcast(int64_t object, int64_t except_this_local_id);
void AnswerRequestObjectPath(int64_t object, int64_t distance);
void AnswerQueryObject(struct hive_connection *con, void *shifted_buffer, int64_t object_id, int64_t offset, int64_t size);
void AnswerPushObject(struct hive_connection *con, int64_t object_id, int64_t offset, int64_t size, int64_t hash);
void RequestPathToIDBroadcast(int64_t global_id, int64_t except_this_local_id);
void AnswerRequestPathToID(int64_t global_id, int64_t distance);

struct hive_connection *GetConnectionById(int64_t local_id, int64_t *index);


#endif
