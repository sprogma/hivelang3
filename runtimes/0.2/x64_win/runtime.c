#define UNICODE 1
#define _UNICODE 1
#define _WIN32_WINNT _WIN32_WINNT_VISTA

#include "inttypes.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include "windows.h"

#include "runtime_lib.h"
#include "remote.h"
#include "runtime.h"

struct defined_array *defined_arrays;

struct worker_info Workers[100] = {};

struct jmpbuf ShedulerBuffer;
int64_t runningId = -1;

SRWLOCK wait_list_lock = SRWLOCK_INIT;
struct waiting_worker *wait_list[10000];
int64_t wait_list_len = 0;

SRWLOCK queue_lock = SRWLOCK_INIT;
struct queued_worker *queue[10000];
int64_t queue_len = 0;


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


void WaitListWorker(struct waiting_worker *t)
{
    AcquireSRWLockExclusive(&wait_list_lock);
    wait_list[wait_list_len++] = t;
    log("Worker add to wait list [next=%p]\n", t->ptr);
    ReleaseSRWLockExclusive(&wait_list_lock);
}

void EnqueueWorkerFromWaitList(struct waiting_worker *w, int64_t rdi_value)
{
    struct queued_worker *t = myMalloc(sizeof(*t));
    t->id = w->id;
    t->ptr = w->ptr;
    t->rbpValue = w->rbpValue;
    t->rdiValue = rdi_value;
    memcpy(t->context, w->context, sizeof(t->context));
    EnqueueWorker(t);
}

void EnqueueWorker(struct queued_worker *t)
{
    AcquireSRWLockExclusive(&queue_lock);
    queue[queue_len++] = t;
    log("Worker enqueued [next=%p]\n", t->ptr);
    ReleaseSRWLockExclusive(&queue_lock);
}

void UpdateWaitingWorkers()
{
    int64_t ticks;
    QueryPerformanceCounter((void *)&ticks);
    log("wait list: %lld\n", wait_list_len);
    AcquireSRWLockExclusive(&wait_list_lock);
    for (int i = 0; i < wait_list_len; ++i)
    {
        struct waiting_worker *w = wait_list[i];
        log("waiting type %lld\n", (int64_t)w->waiting_data->type);
        switch (w->waiting_data->type)
        {
            case WAITING_PUSH:
            {
                struct waiting_push *cause = (struct waiting_push *)w->waiting_data;
                struct object *obj = (void *)GetHashtable(&local_objects, (BYTE *)&cause->object_id, 8, 0);
                if (obj == 0)
                {
                    // remote object, repeat request, with timeout
                    log("waiting for remote push %lld/%lld\n", ticks, cause->repeat_timeout);
                    if (ticks > cause->repeat_timeout)
                    {
                        RequestObjectSet(cause->object_id, cause->offset, myAbs(cause->size), cause->data);
                        cause->repeat_timeout = SheduleTimeoutFromNow(PUSH_REPEAT_TIMEOUT);
                    }
                    break;
                }
                else
                {
                    log("ERROR: waiting for push to local\n");
                    break;
                }
            }
            case WAITING_QUERY:
            {
                struct waiting_query *cause = (struct waiting_query *)w->waiting_data;
                struct object *obj = (void *)GetHashtable(&local_objects, (BYTE *)&cause->object_id, 8, 0);
                if (obj == 0)
                {
                    // remote object, repeat request, with timeout
                    log("waiting for remote query %lld/%lld\n", ticks, cause->repeat_timeout);
                    if (ticks > cause->repeat_timeout)
                    {
                        RequestObjectGet(cause->object_id, cause->offset, myAbs(cause->size));
                        cause->repeat_timeout = SheduleTimeoutFromNow(QUERY_REPEAT_TIMEOUT);
                    }
                    break;
                }
                else
                {
                    log("waiting for local %lld\n", cause->object_id);
                    int64_t rdiValue;
                    if (QueryLocalObject(cause->destination, obj, cause->offset, cause->size, &rdiValue))
                    {
                        EnqueueWorkerFromWaitList(w, rdiValue);
                        myFree(cause);
                        myFree(w);
                        wait_list[i] = wait_list[--wait_list_len];
                        i--;
                        break;
                    }
                    break;
                }
            }
            case WAITING_TIMER:
                print("NOT IMPLEMENTED WAITING_TIMER\n"); break;
            
            case WAITING_PAGES:
            {
                print("NOT IMPLEMENTED WAITING_PAGES\n"); break;
            }
        }
    }
    ReleaseSRWLockExclusive(&wait_list_lock);
}

void SheduleWorker()
{
    setjmpUN(&ShedulerBuffer);

    static int64_t prevPrint = 0;
    int64_t now = GetTicks();
    if (now - prevPrint > MicrosecondsToTicks(100000))
    {
        print("[info]: Wait|Queued %lld|%lld\n", wait_list_len, queue_len);
        prevPrint = now;
    }

    // UpdateWaitingWorkers();
    
    // call next worker
    if (queue_len > 0)
    {
        log("\nSheduling new worker\n");
        AcquireSRWLockExclusive(&queue_lock);
        --queue_len;
        struct queued_worker *copy = queue[queue_len];
        ReleaseSRWLockExclusive(&queue_lock);
        log("Continue worker %lld from %p [rdi=%llx] [context=%p] [rbp=%p]\n", 
                copy->id, copy->ptr, copy->rdiValue, copy->context, copy->rbpValue);

        runningId = copy->id;
        if (Workers[runningId].isDllCall)
        {
            // TODO: lock interrupts mutex
            
            // prepare data
            struct dll_call_data *data = Workers[runningId].ptr;
            
            void *args = (void *)copy->rdiValue;
            int64_t result_promise_id = 0;
            
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
                *cd++ = (int64_t)output;
            }
            else if (data->output_size != -1)
            {
                output = __builtin_alloca(data->output_size);
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
                        memcpy(cd, args, data->sizes[i]);
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
                result_promise_id = *(int64_t*)args;
                args += 8;
            }

            log("calling worker %lld\n", runningId);
            for (int64_t i = 0; i < data->sizes_len; ++i)
            {
                log("ARG[%lld] = %lld\n", i, call_data[i]);
            }
            log("output is %p [->to promise %lld]\n", output, result_promise_id);

            DllCall(copy->ptr, call_data, output);

            log("returned + Error=%lld\n", (int64_t)GetLastError());

            if (output != NULL)
            {
                RequestObjectSet(result_promise_id, 0, data->output_size, output);
            }
        }
        else
        {
            // log("context=");
            // for (int64_t i = 0; i < (int64_t)sizeof(copy->context); ++i)
            // {
            //     log("%02x ", ((BYTE *)copy->context)[i]);
            // }
            // log("\n");
            ExecuteWorker(
                copy->ptr,
                copy->rdiValue,
                copy->rbpValue,
                (BYTE *)copy->context
            );
        }
    }
    
    // pass: check for available to run
    UpdateWaitingWorkers();
}

void StartNewWorker(int64_t workerId, BYTE *inputTable, int64_t except_this_local_id)
{
    /* if we are running too many tasks - redirect new worker to another hive */
    int64_t rnd = 0;
    BCryptGenRandom(NULL, (BYTE *)&rnd, 8, BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    AcquireSRWLockShared(&wait_list_lock);
    if (rnd % 100 < 5 * (wait_list_len + queue_len || except_this_local_id == -1) && except_this_local_id != -3)
    {
        ReleaseSRWLockShared(&wait_list_lock);
        /* select random connection */
        AcquireSRWLockShared(&connections_lock);
        if (connections_len != 0)
        {
            int64_t t = rnd % connections_len;
            log("Want run remote, but: %lld %lld [con=%p]\n", connections[t]->wait_list_len, connections[t]->queue_len, connections[t]);
            if (connections[t]->outgoing != INVALID_SOCKET && 
                connections[t]->local_id != except_this_local_id &&
                ((connections[t]->wait_list_len < 50 && connections[t]->queue_len < 10) || 
                  connections[t]->queue_len == 0))
            {
                StartNewWorkerRemote(connections[t], workerId, inputTable);
                ReleaseSRWLockShared(&connections_lock);
                return;
            }
        }
        ReleaseSRWLockShared(&connections_lock);
    }
    else
    {
        ReleaseSRWLockShared(&wait_list_lock);
    }

    log("Starting new local worker %lld [input table %p]\n", workerId, inputTable);

    // TODO: remove 2048 body size constant
    int64_t tableSize = Workers[workerId].inputSize;
    void *data = myMalloc(tableSize + 2048);
    memcpy(data, inputTable, tableSize);
    
    struct queued_worker *t = myMalloc(sizeof(*t));
    t->id = workerId;
    t->ptr = Workers[workerId].ptr;
    t->rdiValue = (int64_t)data;
    t->rbpValue = (BYTE *)data + Workers[workerId].inputSize;
    memset(t->context, 0, sizeof(t->context));

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

    StartNewWorker(workerId, param, -2);
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
                // read worker id
                int64_t id = *(int64_t *)pos;
                pos += 8;
                // read library name
                char lib_name[256];
                {
                    int64_t sz = *(int64_t *)pos;
                    pos += 8;
                    memcpy(lib_name, pos, sz);
                    lib_name[sz] = 0;
                    pos += sz;
                }
                // read entry name
                char entry[256];
                {
                    int64_t sz = *(int64_t *)pos;
                    pos += 8;
                    memcpy(entry, pos, sz);
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
                        memcpy(data_copy, data, rawsize);
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
                        print("Error: runtime doesn't support string table encoding: %lld\n", (int64_t)encoding);
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
    ////////////////////////// loading stage
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
        print("Error: can't open ../../../res.bin file\n");
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



    ////////////////////////// running stage
    
    InitInternalStructures();
    start_remote_subsystem();
    
    // run first worker with comand line arguments as i64 array

    int64_t inputLen = 0, connectingToMain = 0;
    int64_t inputId = 0;
    int64_t resCodeId = 0;
    int argc;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argc > 1 && argv[1][0] == 'n')
    {
        connectingToMain = 1;
        print("Starting without any workers\n");
    }
    else
    {
        #ifdef _DEBUG
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
        for (int64_t i = 0; i < inputLen; ++i)
        {
            input[i] = myScanI64();
        }
        #endif

        print("waiting startup pages\n");

        while (inputId == 0)
        {
            inputId = NewObject(3, 8 * inputLen, 8, NULL, NULL);
            Sleep(10);
        }

        struct object *obj = (void *)GetHashtable(&local_objects, (BYTE *)&inputId, 8, 0);
        if (obj == 0)
        {
            print("Error: allocated array isn't local\n");
        }
        memcpy(obj, input, 8 * inputLen);
        
        
        while (resCodeId == 0)
        {
            resCodeId = NewObject(2, 4, 4, NULL, NULL);
            Sleep(10);
        }

        BYTE *tbl = myMalloc(16);
        memcpy(tbl + 0, &inputId, 8);
        memcpy(tbl + 8, &resCodeId, 8);

        log("resCodeId=%p\n", resCodeId);
        StartNewWorker(0, tbl, -3);

        myFree(tbl);
    }
        
    print("Running...\n");

    while (connectingToMain || queue_len > 0 || wait_list_len > 0)
    {
        SheduleWorker();
    }

    // TODO: create new dump analog
    // log("At end objects:\n");
    // for (int i = 0; i < object_array_len; ++i)
    // {
    //     log("%02x=[%llx]", i, object_array[i]); PrintObject(object_array[i]);
    // }
    
    if (wait_list_len != 0)
    {
        print("!!!There is %lld more workers in wait_list!!!\n", wait_list_len);
    }
    
    print("Program finished\n");
    struct object_promise *p = (void *)GetHashtable(&local_objects, (BYTE *)&resCodeId, 8, 0);
    if (p == NULL)
    {
        print("Result promise not found on machine\n");
    }
    else
    {
        p = (void *)((BYTE *)p - DATA_OFFSET(*p));
        if (p->ready)
        {
            log("Program exited with code %llx\n", *(int *)p->data);

            #ifndef NDEBUG
            print("Press Ctrl+C to end\n");
            while (1){};
            #endif
            
            ExitProcess(*(int *)p->data);
        }
        else
        {
            log("Result of main function isn't ready after program end\n");
        }
    }

    #ifndef NDEBUG
    print("Press Ctrl+C to end\n");
    while (1){};
    #endif
    
    ExitProcess(1);
}
