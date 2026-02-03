#ifndef RUNTIME_H
#define RUNTIME_H

#ifndef _WIN32_WINNT
    #define _WIN32_WINNT _WIN32_WINNT_VISTA
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
#include "windows.h"

#include "state.h"
#include "runtime_lib.h"
#include "remote.h"

// ! DON'T MOVE FIELDS [only in both this file and runtime.asm]
struct dll_call_data
{
    void *loaded_function;
    // marshal info
    int64_t output_size;
    int64_t sizes_len;
    int64_t *sizes;
    int64_t call_stack_usage;
};

extern int DllCall(struct dll_call_data *, int64_t *, void *);

extern void callExample(void *);

extern BYTE context[];


struct jmpbuf {BYTE _[80];};
[[noreturn]] extern void longjmpUN(struct jmpbuf *, int64_t val);
extern int64_t setjmpUN(struct jmpbuf *);

struct waiting_worker
{
    // worker id
    int64_t id;
    // worker data [continue address on x64]
    void *data;
    // object awaiting data
    int64_t state;
    void *state_data;
    // registers
    void *rbpValue;
    int64_t context[9];
};


struct queued_worker
{
    int64_t id;
    // worker data [continue address on x64]
    void *data;
    // object awaiting data
    int64_t rdiValue;
    // registers
    void *rbpValue;
    int64_t context[9];
};




void EnqueueWorker(struct queued_worker *t);
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
    int8_t provider;   // [-2]
    int8_t type;       // [-1]
    BYTE data[];
};

struct __attribute__((packed)) object_pipe
{
    SRWLOCK lock;
    int64_t length;
    int64_t position;
    int8_t _[6];
    struct object; // data is circular buffer, of length 'length' and position stored in 'position' field
};

struct __attribute__((packed)) object_array
{
    int64_t length;
    int8_t _[6];
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

struct worker_info
{
    int64_t provider;
    void *data;
    int64_t inputSize;
};
extern struct worker_info Workers[];
struct hive_provider_info
{
    void (*ExecuteWorker)(struct queued_worker *);
    int64_t (*UpdateWaitingWorker)(struct waiting_worker *, int64_t, int64_t *);
    void (*NewObjectUsingPage)(int64_t type, int64_t size, int64_t param, int64_t remote_id);
};
extern struct hive_provider_info Providers[];

struct defined_array
{
    BYTE *start;
    int64_t size;
};

struct thread_data
{
    int64_t number;
    int64_t runningId;
    int64_t completedTasks;
    int64_t prevPrint;
    struct jmpbuf ShedulerBuffer;
};
extern DWORD dwTlsIndex;

extern SRWLOCK wait_list_lock;
extern struct waiting_worker *wait_list[];
extern _Atomic int64_t wait_list_len;

extern SRWLOCK queue_lock;
extern struct queued_worker *queue[];
extern _Atomic int64_t queue_len;

extern struct defined_array *defined_arrays;








/*
    call object global_id:

        0 -> any machine
        1 -> local machine
        2 -> remote machine
        
        if first bit set - then this is global id, and we need to run on it
*/

#define IS_CALL_PARAM_GLOBAL_ID(x) ((x) & 0x8000000000000000LL)



void RegisterObjectWithId(int64_t id, void *object);
int64_t GetNewObjectId(int64_t *result);
void UpdateFromQueryResult(void *destination, int64_t object_id, int64_t offset, int64_t size, BYTE *result_data, int64_t *rdiValue);


// void NewObjectUsingPage(int64_t type, int64_t size, int64_t param, int64_t remote_id);
// void UpdateFromQueryResult(void *destination, int64_t object_id, int64_t offset, int64_t size, BYTE *result_data, int64_t *rdiValue);
// int64_t QueryLocalObject(void *destination, void *object, int64_t offset, int64_t size, int64_t *rdiValue);
// void UpdateLocalPush(void *obj, int64_t offset, int64_t size, void *source);


void EnqueueWorkerFromWaitList(struct waiting_worker *w, int64_t rdi_value);
void StartNewWorker(int64_t workerId, int64_t global_id, BYTE *inputTable);

int64_t StartInitialProcess(int64_t entryWorker, int64_t *cmdArgs, int64_t cmdArgsLen);

#endif
