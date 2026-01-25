#define UNICODE 1
#define _UNICODE 1

#include "inttypes.h"
#include "windows.h"

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


#ifndef NDEBUG
    #define log(f, ...) myPrintf(L ## f, __VA_ARGS__)
#else
    // #define log(f, ...) myPrintf(L ## f, __VA_ARGS__)
    #define log(...)
#endif


struct waiting_worker
{
    int64_t id;
    // return address
    void *ptr;
    // object awaiting data
    void *object; 
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
struct worker_info Workers[100] = {};


struct object *object_array[100000] = {};
int64_t object_array_len = 0;

struct defined_array
{
    BYTE *start;
    int64_t size;
} *defined_arrays;


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
            log("Array of %lld bytes, element of size %lld allocated\n", size, param);
            struct object_array *res = myMalloc(sizeof(*res) + size);
            myMemcpy(res->_, &param, 8);
            res->type = OBJECT_ARRAY;
            res->length = size / param;
            int64_t id = (int64_t)res + DATA_OFFSET(struct object_array);
            object_array[object_array_len++] = (struct object *)id;
            log("[id=%llx]\n", id);
            return id;
        }
        case OBJECT_PROMISE:
        {
            log("Promise for size %lld allocated\n", size);
            struct object_promise *res = myMalloc(sizeof(*res) + size);
            res->type = OBJECT_PROMISE;
            res->ready = 0;
            int64_t id = (int64_t)res + DATA_OFFSET(struct object_promise);
            object_array[object_array_len++] = (struct object *)id;
            log("[id=%llx]\n", id);
            return id;
        }
        case OBJECT_OBJECT:
        {
            log("Class for size %lld allocated\n", size);
            struct object_object *res = myMalloc(sizeof(*res) + size);
            res->type = OBJECT_OBJECT;
            int64_t id = (int64_t)res + DATA_OFFSET(struct object_object);
            object_array[object_array_len++] = (struct object *)id;
            log("[id=%llx]\n", id);
            return id;
        }
        case OBJECT_DEFINED_ARRAY:
        {
            // size is ID
            BYTE *objStart = defined_arrays[size].start;
            int64_t objSize = defined_arrays[size].size;
            log("Defined array for size %lld allocated\n", objSize);
            struct object_array *res = myMalloc(sizeof(*res) + objSize);
            myMemcpy(res->_, &param, 8);
            res->type = OBJECT_ARRAY;
            res->length = objSize / param;
            // fill data
            myMemcpy(res->data, objStart, objSize);
            for (int i = 0; i < objSize; ++i)
            {
                log(" %02x", res->data[i]);
            }
            log("\n");
            int64_t id = (int64_t)res + DATA_OFFSET(struct object_array);
            object_array[object_array_len++] = (struct object *)id;
            log("[id=%llx]\n", id);
            return id;
        }
        default:
            log("Wrong type in NewObject\n");
    }
    return -1;
}


void PushObject(){}


struct jmpbuf ShedulerBuffer;
int64_t runningId = -1;

// called only if query of not ready promise [for now]
__attribute__((sysv_abi))
int64_t QueryObject(void *destination, void *object, int64_t offset, int64_t size, void *returnAddress, void *rbpValue)
{
    /* save context and select next worker */
    struct waiting_worker *t = myMalloc(sizeof(*t));

    myMemcpy(t->context, context, sizeof(t->context));
    t->id = runningId;
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


void PrintObject(struct object *object_ptr)
{
    BYTE *ptr = (BYTE *)object_ptr;
    switch (ptr[-1])
    {
        case OBJECT_PROMISE:
            log("Promise(set=%02x, first4bytes=", ptr[-2]);
            for (int i = 0; i < 4; ++i)
                log("%02x ", ptr[i]);
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
                    log("%02x ", ptr[i * elem + j]);
                log("}");
                if (i != len - 1) log(", ");
            }
            log(")\n");
            break;
        }
        case OBJECT_OBJECT:
            log("Class(first4bytes=");
            for (int i = 0; i < 4; ++i)
                log("%02x ", ptr[i]);
            log(")\n");
            break;
        default:
            log("Object of unknown type: %02x\n", ptr[-1]);
    }
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

    log("\nSheduling new worker\n");

    // call next worker
    if (queue_len > 0)
    {
        --queue_len;
        log("Continue worker %lld from %p [rdi=%llx] [context=%p] [rbp=%p]\n", 
                queue[queue_len]->id, queue[queue_len]->ptr, queue[queue_len]->rdiValue, queue[queue_len]->context, queue[queue_len]->rbpValue);

        runningId = queue[queue_len]->id;
        if (Workers[runningId].isDllCall)
        {
            // TODO: lock interrupts mutex
            
            // prepare data
            struct dll_call_data *data = Workers[runningId].ptr;
            
            void *args = (void *)queue[queue_len]->rdiValue;
            void *result_promise = NULL;
            
            // TODO: remove 16 args limit [use alloca?]
            int64_t call_data[32] = {}, *cd;
            void *output = NULL;
            cd = call_data;

            if (data->output_size != -1 && 
                data->output_size != 1 && 
                data->output_size != 2 && 
                data->output_size != 4 && 
                data->output_size != 8)
            {
                output = __builtin_alloca(data->output_size);
                *cd++ = (int64_t)output;
            }
            
            for (int64_t i = 0; i < data->sizes_len; ++i)
            {
                switch (data->sizes[i])
                {
                    case 1:
                    case 2:
                    case 4:
                    case 8:
                    {
                        *cd = 0;
                        myMemcpy(cd, args, data->sizes[i]);
                        cd++;
                        break;
                    }
                    default:
                    {
                        *cd++ = (int64_t)args;
                        break;
                    }
                }
                args += data->sizes[i];
            }

            if (data->output_size != -1)
            {
                result_promise = *(void **)args;
                args += 8;
            }

            log("calling worker %lld\n", runningId);
            for (int64_t i = 0; i < data->sizes_len; ++i)
            {
                log("ARG[%lld] = %lld\n", i, call_data[i]);
            }
            log("output is %p [->to promise %p]\n", output, result_promise);

            DllCall(
                queue[queue_len]->ptr,
                call_data,
                (output == NULL || data->output_size == -1 ? result_promise : output)
            );

            // MessageBox(0, L"Text", L"Caption", 0x40);

            log("returned + Error=%lld\n", (int64_t)GetLastError());

            if (output != NULL)
            {
                myMemcpy(result_promise, output, data->output_size);
                myFree(output);
            }
            else
            {
                PrintObject(result_promise);
            }
            // TODO: free interrupts mutex
        }
        else
        {
            ExecuteWorker(
                queue[queue_len]->ptr,
                queue[queue_len]->rdiValue,
                queue[queue_len]->rbpValue,
                (BYTE *)queue[queue_len]->context
            );
        }
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
                struct queued_worker *new_item = myMalloc(sizeof(*new_item));
                new_item->id = w->id;
                new_item->ptr = w->ptr;
                myMemcpy(new_item->context, w->context, sizeof(new_item->context));
                if (w->size < 0)
                {
                    new_item->rdiValue = 0;
                    myMemcpy(&new_item->rdiValue, p->data, -w->size);
                }
                else
                {
                    myMemcpy(w->destination, p->data, w->size);
                    // not set rdi
                    new_item->rdiValue = 0;
                }
                new_item->rbpValue = w->rbpValue;
                EnqueueWorker(new_item);
                
                myFree(w);
                
                wait_list[i] = wait_list[--wait_list_len];
                i--;
            }
        }
    }
}

void StartNewWorker(int64_t workerId, BYTE *inputTable)
{
    log("Starting new worker %lld [input table %p]\n", workerId, inputTable);
    
    struct queued_worker *t = myMalloc(sizeof(*t));
    t->id = workerId;
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
        log("%02x ", param[i]);
    }
    log("\n");

    // TODO: remove 512 body size constant
    void *data = myMalloc(tableSize + 1024);
    myMemcpy(data, param, tableSize);

    StartNewWorker(workerId, data);
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
    myMemcpy(mem, file + codePosition, fileLength - codePosition);
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
                // read worker id
                int64_t id = *(int64_t *)pos;
                pos += 8;
                // read library name
                char lib_name[256];
                {
                    int64_t sz = *(int64_t *)pos;
                    pos += 8;
                    myMemcpy(lib_name, pos, sz);
                    lib_name[sz] = 0;
                    pos += sz;
                }
                // read entry name
                char entry[256];
                {
                    int64_t sz = *(int64_t *)pos;
                    pos += 8;
                    myMemcpy(entry, pos, sz);
                    entry[sz] = 0;
                    pos += sz;
                }
                // read argument sizes
                int64_t totalSize = 8;
                int64_t output_size = *(int64_t *)pos;
                pos += 8;
                int64_t sizes_len = *(int64_t *)pos;
                pos += 8;
                int64_t *sizes;
                {
                    sizes = myMalloc(sizeof(*sizes) * (sizes_len + 1));
                    for (int64_t i = 0; i < sizes_len; ++i)
                    {
                        sizes[i] = *(int64_t *)pos;
                        totalSize += sizes[i];
                        pos += 8;
                    }
                }
                // set information
                struct dll_call_data *data = myMalloc(sizeof(*data));

                // int64_t num_chars = strlen(lib_name) + 1;
                // wchar_t *utf16 = myMalloc(sizeof(*utf16) * num_chars);
                // MultiByteToWideChar(CP_UTF8, 0, lib_name, -1, utf16, num_chars);

                HINSTANCE lib = LoadLibraryA(lib_name);
                data->loaded_function = GetProcAddress(lib, entry);
                
                data->output_size = output_size;
                data->sizes_len = sizes_len;
                data->sizes = sizes;
                data->call_stack_usage = 32 + 16 * (sizes_len < 4 ? 0 : (sizes_len - 4 + 1) / 2);
                Workers[id] = (struct worker_info){1, data, totalSize};
                
                // log data
                log("worker %lld is dll call of library %s %s -> result function is %p\n", id, lib_name, entry, data->loaded_function);
                log("stack usage: %lld\n", data->call_stack_usage);
                log("output have size %lld [and args of total size %lld]\n", output_size, totalSize);
                for (int64_t i = 0; i < sizes_len; ++i)
                {
                    log("argument %lld have size %lld\n", i, sizes[i]);
                }                
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
                    Workers[id] = (struct worker_info){0, ptr, tableSize};
                    log("Worker %lld have been loaded to %p [offset %llx] with input table of size %lld\n", id, ptr, offset, tableSize);
                }
                break;
            }
            case 17: // String table
            {
                // read table size
                int64_t size = *(int64_t *)pos;
                pos += 8;
                // read data encoding
                int8_t encoding = *pos++;
                
                defined_arrays = myMalloc(sizeof(*defined_arrays) * size);
                switch (encoding)
                {
                    case 0x0:
                    {
                        // read raw data size
                        int64_t rawsize = *(int64_t *)pos;
                        pos += 8;
                        // raw bytes
                        BYTE *data = pos + (size * 16); // pos + header size
                        BYTE *data_copy = myMalloc(rawsize);
                        myMemcpy(data_copy, data, rawsize);
                        // read table header [offset+size]
                        for (int64_t i = 0; i < size; ++i)
                        {
                            int64_t el_offset = *(int64_t *)pos;
                            pos += 8;
                            int64_t el_size = *(int64_t *)pos;
                            pos += 8;
                            defined_arrays[i].start = data_copy + el_offset;
                            defined_arrays[i].size = el_size;
                        }
                        pos += rawsize;
                        break;
                    }
                    default:
                        myPrintf(L"Error: runtime doesn't support string table encoding: %lld\n", (int64_t)encoding);
                        return NULL;
                }
                break;
            }
            default:
                log("Error: unknown header type %lld\n", (int64_t)type);
                break;
        }
    }
    return mem;
}


int entry()
{
    init_lib();
    
    HANDLE hFile = CreateFile(
        L"../../../res.bin",
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (hFile == INVALID_HANDLE_VALUE) {
        myPrintf(L"Error: can't open ../../../res.bin file\n");
        return 1;
    }

    LARGE_INTEGER fileSize;
    GetFileSizeEx(hFile, &fileSize);
    int64_t len = fileSize.QuadPart;

    BYTE *buf = myMalloc(len);
    DWORD bytesRead = 0;
    ReadFile(
        hFile,
        buf,
        len,
        &bytesRead,
        NULL
    );

    CloseHandle(hFile);
    
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
        log("%02x ", ((BYTE *)res)[i]);
    }
    log("\n");

    // run first worker with comand line arguments as i32 array

    int64_t inputLen = 0;
    #ifdef _DEBUG
    int argc;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    inputLen = argc - 1;
    int64_t *input = myMalloc(8 * inputLen);
    log("READING INPUT AS: ");
    for (int i = 1; i < argc; ++i)
    {
        input[i - 1] = myAtoll(argv[i]);
        log("%lld ", input[i - 1]);
    }
    log("\n");
    #else
    inputLen = myScanI64();
    int64_t *input = myMalloc(8 * inputLen);
    for (int i = 0; i < inputLen; ++i)
    {
        input[i] = myScanI64();
    }
    #endif

    int64_t inputId = NewObject(3, 8 * inputLen, 8);
    myMemcpy((void *)inputId, input, 8 * inputLen);
    
    int64_t resCodeId = NewObject(2, 4, 4);

    BYTE *tbl = myMalloc(16 + 2048);
    myMemcpy(tbl + 0, &inputId, 8);
    myMemcpy(tbl + 8, &resCodeId, 8);
    
    StartNewWorker(0, tbl);
        
    log("Running...\n");

    while (queue_len > 0)
    {
        SheduleWorker();
    }

    log("At end objects:\n");
    for (int i = 0; i < object_array_len; ++i)
    {
        log("%02x=[%llx]", i, object_array[i]); PrintObject(object_array[i]);
    }
    
    if (wait_list_len != 0)
    {
        myPrintf(L"!!!ALL EXISTING WORKERS ARE DEADLOCKED!!!\n");
    }

    struct object_promise *p = (struct object_promise *)(resCodeId - DATA_OFFSET(struct object_promise));
    if (p->ready)
    {
        log("Program exited with code %llx\n", *(int *)p->data);
        ExitProcess(*(int *)p->data);
    }
    log("Result of main function isn't ready after program end\n");
    ExitProcess(1);
}
