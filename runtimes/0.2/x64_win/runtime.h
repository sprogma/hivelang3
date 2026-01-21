#ifndef RUNTIME_H
#define RUNTIME_H

#ifndef _WIN32_WINNT
    #define _WIN32_WINNT _WIN32_WINNT_VISTA
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
#include "windows.h"

#include "runtime_lib.h"
#include "runtime_lib.h"


extern int fastPushObject(void);
extern int fastQueryObject(void);
extern int fastNewObject(void);
extern int fastCallObject(void);

extern int ExecuteWorker(void *, int64_t, void *, BYTE *);

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
extern void longjmpUN(struct jmpbuf *, int64_t val);
extern int64_t setjmpUN(struct jmpbuf *);


struct waiting_worker
{
        int64_t id;
    // return address
    void *ptr;
    // object awaiting data
    int64_t object; 
    void *destination;
    int64_t offset;
    int64_t size;
    // registers
    void *rbpValue;
    int64_t context[9];
};


struct queued_worker
{
        int64_t id;
    // return address
    void *ptr;
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


#define DATA_OFFSET(T) ((int64_t)&(((T *)NULL)->data))


struct object
{
    int8_t type;
    BYTE data[];
};

struct object_array
{
    int64_t length;
    int8_t _[7];
    int8_t type;
    BYTE data[];
}; __attribute__((packed));

struct object_promise
{
    int8_t ready;
    int8_t type;
    BYTE data[];
}; __attribute__((packed));

struct object_object
{
    int8_t type;
    BYTE data[];
}; __attribute__((packed));

struct worker_info
{
    int64_t isDllCall;
    void *ptr;
    int64_t inputSize;
};
extern struct worker_info Workers[];


extern struct object *object_array[];
extern int64_t object_array_len;

struct defined_array
{
    BYTE *start;
    int64_t size;
};

extern struct jmpbuf ShedulerBuffer;
extern int64_t runningId;

extern struct waiting_worker *wait_list[];
extern int64_t wait_list_len;

extern struct queued_worker *queue[];
extern int64_t queue_len;

extern struct defined_array *defined_arrays;
















__attribute__((sysv_abi))
int64_t QueryObject(void *destination, int64_t object, int64_t offset, int64_t size, void *returnAddress, void *rbpValue);

__attribute__((sysv_abi))
void PushObject(int64_t object, void *source, int64_t offset, int64_t size, void *returnAddress, void *rbpValue);

int64_t NewObject(int64_t type, int64_t size, int64_t param);

#endif
