#include "inttypes.h"
#include "inttypes.h"
#include "stdio.h"
#include "windows.h"

extern int fastPushObject(void);
extern int fastQueryObject(void);
extern int fastNewObject(void);
extern int fastCallObject(void);

extern int ExecuteWorker(void *, int64_t, void *, BYTE *);

extern void callExample(void *);

extern BYTE context[];


struct jmpbuf {BYTE _[80];};
extern void longjmpUN(struct jmpbuf *, int64_t val);
extern int64_t setjmpUN(struct jmpbuf *);


#ifndef NDEBUG
    #define log(...) printf(__VA_ARGS__)
#else
    #define log(...)
#endif


struct waiting_worker
{
    int64_t context[5]; // registers
    // return address
    void *ptr;
    // object awaiting data
    void *object; 
    void *destination;
    int64_t offset;
    int64_t size;
    // registers
    void *rbpValue;
};


struct queued_worker
{
    int64_t context[5]; // registers
    // return address
    void *ptr;
    // object awaiting data
    int64_t rdiValue;
    void *rbpValue;
};




void EnqueueWorker(struct queued_worker *t);
void WaitListWorker(struct waiting_worker *t);




typedef uint8_t BYTE;


#define OBJECT_PIPE    0x01
#define OBJECT_PROMISE 0x02
#define OBJECT_ARRAY   0x03
#define OBJECT_OBJECT  0x04


#define DATA_OFFSET(T) ((int64_t)&(((T *)NULL)->data))


struct object
{
    int8_t type;
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

struct worker_info
{
    void *ptr;
    int64_t inputSize;
};
struct worker_info Workers[10] = {};


struct object *object_array[1000] = {};
int64_t object_array_len = 0;


// if allocating ARRAY, param must be element size.
// [it can be used to split big arrays on diffrent hives]
// if allocating OBJECT, param = 1
// if allocating PROMISE/PIPE, param is unused
int64_t NewObject(int64_t type, int64_t size, int64_t param)
{
    switch (type)
    {
        case OBJECT_ARRAY:
        {
            log("Object of %lld bytes, element of size %lld allocated\n", size, param);
            struct object_array *res = malloc(sizeof(*res) + size);
            memcpy(res->_, &param, 8);
            res->type = type;
            res->length = size / param;
            int64_t id = (int64_t)res + DATA_OFFSET(struct object_array);
            object_array[object_array_len++] = (struct object *)id;
            log("[id=%016llx]\n", id);
            return id;
        }
        case OBJECT_PROMISE:
        {
            log("Promise for size %lld allocated\n", size);
            struct object_promise *res = malloc(sizeof(*res) + size);
            res->type = type;
            res->ready = 0;
            int64_t id = (int64_t)res + DATA_OFFSET(struct object_promise);
            object_array[object_array_len++] = (struct object *)id;
            log("[id=%016llx]\n", id);
            return id;
        }
        default:
            log("Wrong type in NewObject\n");
    }
    return -1;
}


void PushObject(){}


struct jmpbuf ShedulerBuffer;

// called only if query of not ready promise [for now]
__attribute__((sysv_abi))
int64_t QueryObject(void *destination, void *object, int64_t offset, int64_t size, void *returnAddress, void *rbpValue)
{
    /* save context and select next worker */
    struct waiting_worker *t = malloc(sizeof(*t));

    memcpy(t->context, context, sizeof(t->context));
    t->ptr = returnAddress;
    t->rbpValue = rbpValue;
    t->size = size;
    t->offset = offset;
    t->destination = destination;
    t->object = object;
    
    log("Awaiting promise at %p\n", object);

    WaitListWorker(t);
    
    longjmpUN(&ShedulerBuffer, 1);
    
    return 0;
}

struct waiting_worker *wait_list[100];
int64_t wait_list_len = 0;

struct queued_worker *queue[100];
int64_t queue_len = 0;

void WaitListWorker(struct waiting_worker *t)
{
    wait_list[wait_list_len++] = t;
    log("Worker add to wait list [next=%p]\n", t->ptr);
}

void EnqueueWorker(struct queued_worker *t)
{
    queue[queue_len++] = t;
    log("Worker enqueued [next=%p]\n", t->ptr);
}

void SheduleWorker()
{
    setjmpUN(&ShedulerBuffer);

    log("Sheduling new worker\n");

    // call next worker
    if (queue_len > 0)
    {
        --queue_len;
        log("Continue worker from %p [rdi=%016llX] [context=%p] [rbp=%p]\n", 
                queue[queue_len]->ptr, queue[queue_len]->rdiValue, queue[queue_len]->context, queue[queue_len]->rbpValue);
        ExecuteWorker(
            queue[queue_len]->ptr,
            queue[queue_len]->rdiValue,
            queue[queue_len]->rbpValue,
            (BYTE *)queue[queue_len]->context
        );
    }
    // check for new available
    for (int i = 0; i < wait_list_len; ++i)
    {
        struct waiting_worker *w = wait_list[i];
        if (((BYTE*)w->object)[-1] == OBJECT_PROMISE)
        {
            struct object_promise *p = (struct object_promise *)(w->object - DATA_OFFSET(struct object_promise));
            if (p->ready)
            {
                struct queued_worker *new_item = malloc(sizeof(*new_item));
                new_item->ptr = w->ptr;
                memcpy(new_item->context, w->context, sizeof(new_item->context));
                if (w->size < 0)
                {
                    new_item->rdiValue = 0;
                    memcpy(&new_item->rdiValue, p->data, -w->size);
                }
                else
                {
                    memcpy(w->destination, p->data, w->size);
                    // not set rdi
                    new_item->rdiValue = 0;
                }
                new_item->rbpValue = w->rbpValue;
                EnqueueWorker(new_item);
                
                free(w);
                
                wait_list[i] = wait_list[--wait_list_len];
                i--;
            }
        }
    }
}

void StartNewWorker(int64_t workerId, BYTE *inputTable)
{
    log("Starting new worker %lld [input table %p]\n", workerId, inputTable);
    
    struct queued_worker *t = malloc(sizeof(*t));
    t->ptr = Workers[workerId].ptr;
    t->rdiValue = (int64_t)inputTable;
    t->rbpValue = inputTable + Workers[workerId].inputSize;

    EnqueueWorker(t);
}

void CallObject(BYTE *param, int64_t workerId)
{
    int64_t tableSize = Workers[workerId].inputSize;
    
    log("Calling worker %lld [data=%p]\n", workerId, param);
    log("Table = ");
    for (int64_t i = 0; i < tableSize; ++i)
    {
        log("%02X ", param[i]);
    }
    log("\n");

    void *data = malloc(tableSize + 512);
    memcpy(data, param, tableSize);

    StartNewWorker(workerId, data);
}


void PrintObject(struct object *object_ptr)
{
    BYTE *ptr = (BYTE *)object_ptr;
    switch (ptr[-1])
    {
        case OBJECT_PROMISE:
            log("Promise(set=%d, first4bytes=", ptr[-2]);
            for (int i = 0; i < 4; ++i)
                log("%02X ", ptr[i]);
            log(")\n");
            break;
        case OBJECT_ARRAY:
        {
            int64_t len = ((uint64_t *)ptr)[-2];
            int64_t elem = ((uint64_t *)ptr)[-1] & 0x00FFFFFFFFFFFFFF;
            log("Array(length=%lld, element_size=%lld, ", len, elem);
            for (int i = 0; i < len; ++i)
            {
                log("{ ");
                for (int j = 0; j < elem; ++j)
                    log("%02X ", ptr[i * elem + j]);
                log("}");
                if (i != len - 1) log(", ");
            }
            log(")\n");
            break;
        }
        default:
            log("Object of unknown type: %d\n", ptr[-1]);
    }
}


enum relocation_type
{
    DYNAMIC_SYMBOL,
    QUERY_OBJECT,
    PUSH_OBJECT,
    NEW_OBJECT,
    CALL_OBJECT,
};


#define RELOCATION_32BIT        0x0001
#define RELOCATION_64BIT        0x0002
#define RELOCATION_RELATIVE     0x0010
#define RELOCATION_NOT_RELATIVE 0x0020


// executable structure 0.1:
/*

    prefix
        "HIVE" ?

    version
        i64 // = main_version * 1000 + low_version
    
    header:
        i64 address of code

        array of external symbols: [all space up to start of code]
            i8 symbol_type
            ... [[data]]
    code:
        raw bytes

*/
void *LoadWorker(BYTE *file, int64_t fileLength, int64_t *res_len)
{
    /* read prefix */
    if (file[0] != 'H' || file[1] != 'I' || file[2] != 'V' || file[3] != 'E')
    {
        log("Error: this isn't hive executable\n");
        return NULL;
    }
    uint64_t version = *(uint64_t *)&file[4];
    if (version / 1000 != 0)
    {
        log("Error: this is executable of not 0 version [%llu]\n", version / 1000);
        return NULL;
    }
    uint64_t codePosition = *(uint64_t *)&file[12];
    /* read code */
    void *mem = VirtualAlloc(NULL, fileLength - codePosition, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (mem == NULL)
    {
        log("Error: winapi error %ld\n", GetLastError());
        return NULL;
    }
    memcpy(mem, file + codePosition, fileLength - codePosition);
    if (res_len) { *res_len = fileLength - codePosition; }
    /* read header */
    BYTE *pos = file + 20;
    while (pos < codePosition + file)
    {
        BYTE type = *pos++;
        switch (type)
        {
            case 0: // PushObject
            case 1: // QueryObject
            case 2: // NewObject
            case 3: // CallObject
            {
                // read positions and replace calls
                int64_t count = *(int64_t *)pos;
                pos += 8;
                for (int64_t i = 0; i < count; ++i)
                {
                    log("set to %lld ", *(int64_t *)pos);
                    uint64_t *callPosition = (uint64_t *)(mem + *(int64_t *)pos);
                    pos += 8;
                    switch (type)
                    {
                        case 0: *callPosition = (uint64_t)&fastPushObject; break;
                        case 1: *callPosition = (uint64_t)&fastQueryObject; break;
                        case 2: *callPosition = (uint64_t)&fastNewObject; break;
                        case 3: *callPosition = (uint64_t)&fastCallObject; break;
                    }
                    log("ptr=%p\n", (void *)*callPosition);
                }
                break;
            }
            case 4: // DLL call
                /*   
                    i64 name_len
                    byte[] name
                    i64 positions length
                    i64[] positions
                */
                log("Error: DLL Calls are unsupported in version 0.1\n");
                break;
            case 16: // Worker positions
            {
                int64_t count = *(int64_t *)pos;
                pos += 8;
                for (int64_t i = 0; i < count; ++i)
                {
                    // read id, position and input table size
                    int64_t id = *(int64_t *)pos;
                    pos += 8;
                    int64_t offset = *(int64_t *)pos;
                    pos += 8;
                    int64_t tableSize = *(int64_t *)pos;
                    pos += 8;
                    // set data
                    void *ptr = mem + offset;
                    Workers[id] = (struct worker_info){ptr, tableSize};
                    log("Worker %lld have been loaded to %p [offset %016llx] with input table of size %lld\n", id, ptr, offset, tableSize);
                }
                break;
            }
            default:
                log("Error: unknown header type %u\n", type);
                break;
        }
    }
    return mem;
}


int main(int argc, char **argv)
{
    (void) argc;
    (void) argv;
    // read file
    FILE *f = fopen("../../../res.bin", "rb");
    if (f == NULL)
    {
        log("Error: file doesn't exists\n");
        return 1;
    }
    
    BYTE *buf = malloc(1024 * 1024);
    int64_t len = fread(buf, 1, 1024 * 1024, f);
    fclose(f);
    
    // load worker
    int64_t res_len = 0;
    void *res = LoadWorker(buf, len, &res_len);
    if (res == NULL)
    {
        log("Error: at loading file\n");
        return 1;
    }
    
    // print it
    for (int i = 0; i < res_len; ++i)
    {
        log("%02X ", ((BYTE *)res)[i]);
    }
    log("\n");

    // run first worker with comand line arguments as i32 array

    int64_t inputLen = 0;
    #ifdef _DEBUG
    inputLen = argc - 1;
    int64_t *input = malloc(8 * inputLen);
    log("READING INPUT AS: ");
    for (int i = 1; i < argc; ++i)
    {
        input[i - 1] = atoll(argv[i]);
        log("%lld ", input[i]);
    }
    log("\n");
    #else
    scanf("%lld", &inputLen);
    int64_t *input = malloc(8 * inputLen);
    for (int i = 0; i < inputLen; ++i)
    {
        scanf("%lld", input + i);
    }
    log("\n");
    #endif

    int64_t inputId = NewObject(3, 8 * inputLen, 8);
    memcpy((void *)inputId, input, 8 * inputLen);
    
    int64_t resCodeId = NewObject(2, 4, 4);

    BYTE *tbl = calloc(1, 16 + 512);
    memcpy(tbl + 0, &inputId, 8);
    memcpy(tbl + 8, &resCodeId, 8);
    
    StartNewWorker(0, tbl);
        
    log("Running...\n");

    while (queue_len > 0)
    {
        SheduleWorker();
    }

    log("At end objects:\n");
    for (int i = 0; i < object_array_len; ++i)
    {
        log("%d=", i); PrintObject(object_array[i]);
    }

    // for (int i = 0; i < argc - 1; ++i)
    // {
    //     printf("%lld ", ((int64_t *)inputId)[i]);
    // }
    // printf("\n");

    struct object_promise *p = (struct object_promise *)(resCodeId - DATA_OFFSET(struct object_promise));
    if (p->ready)
    {
        log("Program exited with code %d\n", *(int *)p->data);
        return *(int *)p->data;
    }
    log("Result of main function isn't ready after program end\n");
    return 1;
}
