#ifndef RUNTIME_H
#define RUNTIME_H

#ifndef _WIN32_WINNT
    #define _WIN32_WINNT _WIN32_WINNT_VISTA
#endif

#ifndef NDEBUG
    #define unreachable assert(NULL == "This is unreachable code")
#else
    #define unreachable __builtin_unreachable()
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
#include "windows.h"

#include "state.h"
#include "runtime_lib.h"
#include "remote.h"
#include "providers.h"


extern void callExample(void *);

extern int64_t NUM_THREADS;
extern int64_t CHUNK_TIME_US;

#define CONTEXT_SIZE (9+5)

struct jmpbuf {BYTE _[80];};
[[noreturn]] extern void longjmpUN(struct jmpbuf *, int64_t val);
extern int64_t setjmpUN(struct jmpbuf *);

struct waiting_worker
{
    // object waiting data
    int64_t state;
    void *state_data;
    // worker id
    int64_t id;
    int64_t depth;
    // worker data [continue address on x64]
    void *data;
    // registers
    void *rbpValue;
    int64_t context[CONTEXT_SIZE];
};


struct queued_worker
{
    int64_t id;
    int64_t depth;
    // worker data [continue address on x64]
    void *data;
    // object awaiting data
    int64_t rdiValue;
    // registers
    void *rbpValue;
    int64_t context[CONTEXT_SIZE];
};




void WaitListWorker(struct waiting_worker *t);




typedef uint8_t BYTE;


#define OBJECT_PIPE           0x01
#define OBJECT_PROMISE        0x02
#define OBJECT_ARRAY          0x03
#define OBJECT_OBJECT         0x04
#define OBJECT_DEFINED_ARRAY  0x05


#define DATA_OFFSET(T) ((int64_t)&(((typeof(T) *)NULL)->data))


struct __attribute__((packed)) object
{
    uint64_t data_size : 48;
    uint64_t provider : 8;   // [-2]
    uint64_t type : 8;       // [-1]
    BYTE data[];
};

struct __attribute__((packed)) object_pipe
{
    SRWLOCK lock;
    int64_t length;
    int64_t position;
    struct object; // data is circular buffer, of length 'length' and position stored in 'position' field
};

struct __attribute__((packed)) object_array
{
    int64_t length;
    struct object;
};

struct __attribute__((packed)) object_promise
{
    int8_t ready;
    struct object;
};

struct __attribute__((packed)) object_object
{
    struct object;
};


struct defined_array
{
    BYTE *start;
    int64_t size;
};

struct thread_data
{
    int64_t number;
    int64_t runningId;
    int64_t runningDepth;
    struct jmpbuf ShedulerBuffer;
    // special fields
    _Atomic int64_t lastWorkerStart;
    // performance counters
    _Atomic int64_t executedTasks;
    _Atomic int64_t completedTasks;
    _Atomic int64_t stalledTasks;
    _Atomic int64_t stallable;
};

extern DWORD dwTlsIndex;

struct worker_info
{
    int64_t provider;
    void *data;
    int64_t inputSize;
    int64_t affinity;
};
extern struct worker_info Workers[];

struct hive_provider_info
{
    void (*ExecuteWorker)(struct queued_worker *);
    void (*NewObjectUsingPage)(int64_t type, int64_t size, int64_t param, int64_t *remote_id);
    int64_t stallable;
    int64_t (*TryStallWorker)(HANDLE hThread, struct thread_data *data, int64_t runnedTicks);
    void (*StartNewLocalWorker)(int64_t workerId, BYTE *inputTable);
};
extern struct hive_provider_info Providers[];

extern SRWLOCK wait_list_lock;
extern struct waiting_worker *wait_list[];
extern _Atomic int64_t wait_list_len;

extern struct defined_array *defined_arrays;




extern _Atomic int64_t queue_size;

void queue_init();
void queue_enqueue(struct queued_worker *wk);
struct queued_worker *queue_extract(int64_t threadId);




/*
    call object global_id:

        0 -> any machine
        1 -> local machine
        2 -> remote machine
        
        if first bit set - then this is global id, and we need to run on it
*/

#define IS_CALL_PARAM_GLOBAL_ID(x) ((x) & 0x8000000000000000LL)



void UpdateWaitingWorkers();
void RegisterObjectWithId(int64_t id, void *object);
int64_t GetNewObjectId(int64_t *result);
void UpdateFromQueryResult(void *destination, int64_t object_id, int64_t offset, int64_t size, BYTE *result_data, int64_t *rdiValue);


// void NewObjectUsingPage(int64_t type, int64_t size, int64_t param, int64_t *remote_id);
// void UpdateFromQueryResult(void *destination, int64_t object_id, int64_t offset, int64_t size, BYTE *result_data, int64_t *rdiValue);
// int64_t QueryLocalObject(void *destination, void *object, int64_t offset, int64_t size, int64_t *rdiValue);
// void UpdateLocalPush(void *obj, int64_t offset, int64_t size, void *source);


void EnqueueWorkerFromWaitList(struct waiting_worker *w, int64_t rdi_value);
void StartNewWorker(int64_t workerId, int64_t global_id, BYTE *inputTable);

int64_t StartInitialProcess(int64_t entryWorker, int64_t *cmdArgs, int64_t cmdArgsLen);
int64_t ShedulerStart(int64_t resCodeId);

#endif
