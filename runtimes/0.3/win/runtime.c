#define UNICODE 1
#define _UNICODE 1
#define _WIN32_WINNT _WIN32_WINNT_VISTA

#include "inttypes.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include "windows.h"

#include "runtime_lib.h"
#include "runtime_api.h"
#include "remote.h"
#include "runtime.h"

#include "gpu_subsystem.h"
#include "providers.h"

#include "x64/x64.h"
#include "gpu/gpu.h"
#include "dll/dll.h"
#include "loc/loc.h"

struct defined_array *defined_arrays;

struct worker_info Workers[100] = {};
struct hive_provider_info Providers[] = {
    {
        .ExecuteWorker=x64ExecuteWorker,
        .NewObjectUsingPage=x64NewObjectUsingPage,
    },
    {
        .ExecuteWorker=gpuExecuteWorker,
        .NewObjectUsingPage=gpuNewObjectUsingPage,
    },
    {
        .ExecuteWorker=dllExecuteWorker,
        .NewObjectUsingPage=NULL,
    },
    {
        .ExecuteWorker=NULL,
        .NewObjectUsingPage=locNewObjectUsingPage,
    }
};

DWORD dwTlsIndex;

SRWLOCK wait_list_lock = SRWLOCK_INIT;
struct waiting_worker *wait_list[100000];
_Atomic int64_t wait_list_len = 0;

void RegisterObjectWithId(int64_t id, void *object)
{
    SetHashtable(&local_objects, (BYTE *)&id, 8, (int64_t)object);
}

int64_t GetNewObjectId(int64_t *result)
{
    int64_t remote_id = 0, set = 0;
    AcquireSRWLockShared(&pages_lock);
    for (int64_t i = 0; i < pages_len; ++i)
    {
        if (pages[i].next_allocated_id < OBJECTS_PER_PAGE)
        {
            int64_t t = pages[i].next_allocated_id++;
            if (t < OBJECTS_PER_PAGE)
            {
                remote_id = (pages[i].id << 24) | t;
                set = 1;
                break;
            }
        }
    }
    ReleaseSRWLockShared(&pages_lock);

    *result = remote_id;
    return set;
}


void universalPauseWorker(void *returnAddress, void *rbpValue, enum worker_wait_state state, void *state_data)
{
    struct thread_data* lc_data = TlsGetValue(dwTlsIndex);
    switch (Workers[lc_data->runningId].provider)
    {
        case PROVIDER_X64:
            x64PauseWorker(returnAddress, rbpValue, state, state_data);
            break;
        case PROVIDER_GPU:
            print("Error: gpu worker doen't support universal pause\n");
            ExitProcess(1);
    }
}

void universalUpdateLocalPush(void *obj, int64_t offset, int64_t size, void *source)
{
    struct object *objj = (void *)((int64_t)obj - DATA_OFFSET(*objj));
    switch (objj->provider)
    {
        case PROVIDER_X64: x64UpdateLocalPush(obj, offset, size, source); break;
    }
}

void UpdateFromQueryResult(void *destination, int64_t object_id, int64_t offset, int64_t size, BYTE *result_data, int64_t *rdiValue)
{
    (void)object_id;
    (void)offset;
    memcpy((size < 0 ? rdiValue : destination), result_data, myAbs(size));
}

void PrintObject(struct object *object_ptr)
{
    BYTE *ptr = (BYTE *)object_ptr;
    switch (ptr[-1])
    {
        case OBJECT_PROMISE:
            log("Promise(set=%02x, first4bytes=", ptr[-3]);
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
    log("Worker add to wait list [id=%lld data=%p]\n", t->id, t->data);
    ReleaseSRWLockExclusive(&wait_list_lock);
}

void EnqueueWorkerFromWaitList(struct waiting_worker *w, int64_t rdi_value)
{
    struct queued_worker *t = myMalloc(sizeof(*t));
    t->id = w->id;
    t->depth = w->depth;
    t->data = w->data;
    t->rbpValue = w->rbpValue;
    t->rdiValue = rdi_value;
    memcpy(t->context, w->context, sizeof(t->context));
    log("Worker enqueued [id=%lld, data=%p]\n", t->id, t->data);
    queue_enqueue(t);
}

void UpdateWaitingWorkers()
{
    int64_t ticks;
    QueryPerformanceCounter((void *)&ticks);
    log("wait list: %lld\n", wait_list_len);
    if (!TryAcquireSRWLockExclusive(&wait_list_lock))
    {
        return;
    }
    BYTE rnd;
    BCryptGenRandom(NULL, &rnd, 1, BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    for (int i = rnd % 16; i < wait_list_len; i += 16)
    {
        struct waiting_worker *w = wait_list[i];
        int64_t res = 0;
        int64_t rdiValue = 0;
        switch (w->state)
        {
            // delarations
            //<<--Quote-->> from::(ls *.c -r|sls "^\s*//@reg\s+(\w+)\s+(\w+)$"|% Matches|%{[pscustomobject]@{a=$_.Groups[1];b=$_.Groups[2]}}|group b|%{$n=$_;$_.Group|%{"$(" "*12)int64_t $($n.Name)(struct waiting_worker *, int64_t, int64_t *);"}})-join"`n"
            int64_t anyCastStates(struct waiting_worker *, int64_t, int64_t *);
            int64_t anyCastStates(struct waiting_worker *, int64_t, int64_t *);
            int64_t anyCastStates(struct waiting_worker *, int64_t, int64_t *);
            int64_t anyCastStates(struct waiting_worker *, int64_t, int64_t *);
            int64_t anyCastStates(struct waiting_worker *, int64_t, int64_t *);
            int64_t x64NewObjectStates(struct waiting_worker *, int64_t, int64_t *);
            int64_t x64PushObjectStates(struct waiting_worker *, int64_t, int64_t *);
            int64_t x64QueryObjectStates(struct waiting_worker *, int64_t, int64_t *);
            //<<--QuoteEnd-->>
            // calls
            //<<--Quote-->> from::(ls *.c -r|sls "^\s*//@reg\s+(\w+)\s+(\w+)$"|% Matches|%{[pscustomobject]@{a=$_.Groups[1];b=$_.Groups[2]}}|group b|%{$n=$_;$_.Group|%{"            case $($_.a):"};"                res = $($n.Name)(w, ticks, &rdiValue); break;"})-join"`n"
            case WK_STATE_GET_OBJECT_SIZE:
            case WK_STATE_GET_OBJECT_SIZE_RESULT:
            case WK_STATE_GET_OBJECT_DATA:
            case WK_STATE_GET_OBJECT_DATA_RESULT:
            case WK_STATE_CAST_WAIT_PAGES:
                res = anyCastStates(w, ticks, &rdiValue); break;
            case WK_STATE_NEW_OBJECT_WAIT_PAGES_X64:
                res = x64NewObjectStates(w, ticks, &rdiValue); break;
            case WK_STATE_PUSH_OBJECT_WAIT_X64:
                res = x64PushObjectStates(w, ticks, &rdiValue); break;
            case WK_STATE_QUERY_OBJECT_WAIT_X64:
                res = x64QueryObjectStates(w, ticks, &rdiValue); break;
            //<<--QuoteEnd-->>
        }
        if (res)
        {
            EnqueueWorkerFromWaitList(w, rdiValue);
            myFree(w);
            wait_list[i] = wait_list[--wait_list_len];
            i--;
            break;
        }
    }
    ReleaseSRWLockExclusive(&wait_list_lock);
}

void SheduleWorker(struct thread_data *lc_data)
{
    setjmpUN(&lc_data->ShedulerBuffer);

    int64_t now = GetTicks();
    if (now - lc_data->prevPrint > MicrosecondsToTicks(100000))
    {
        print("thread %lld:  Wait|Queued %lld|%lld [%lld completed]\n", lc_data->number, wait_list_len, queue.size, lc_data->completedTasks);
        // AcquireSRWLockShared(&wait_list_lock);
        // int64_t cnt[10] = {};
        // for (int64_t i = 0; i < wait_list_len; ++i)
        // {
        //     cnt[wait_list[i]->waiting_data->type]++;
        // }
        // print("wait for WAITING_PUSH:  %lld\n", cnt[WAITING_PUSH]);
        // print("wait for WAITING_QUERY: %lld\n", cnt[WAITING_QUERY]);
        // ReleaseSRWLockShared(&wait_list_lock);
        lc_data->prevPrint = now;
        lc_data->completedTasks = 0;
    }

    // call next worker

    struct queued_worker *curr = queue_extract();
    if (curr)
    {
        lc_data->completedTasks++;
        log("\nSheduling new worker\n");
        log("Continue worker %lld from data=%p [rdi=%llx] [context=%p] [rbp=%p]\n",
                curr->id, curr->data, curr->rdiValue, curr->context, curr->rbpValue);
        lc_data->runningId = curr->id;
        lc_data->runningDepth = curr->depth;
        Providers[Workers[curr->id].provider].ExecuteWorker(curr);
    }
    UpdateWaitingWorkers();
}

void StartNewWorker(int64_t workerId, int64_t global_id, BYTE *inputTable)
{
    /* if we are running too many tasks - redirect new worker to another hive */
    int64_t rnd = 0;
    BCryptGenRandom(NULL, (BYTE *)&rnd, 8, BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    rnd = myAbs(rnd);
    int64_t random_confirm = (rnd & 0x80000000) && (rnd % 100 < 5 * (wait_list_len + queue.size));
    if ((random_confirm || global_id != 0) && global_id != 1)
    {
        /* select random connection */
        AcquireSRWLockShared(&connections_lock);
        if (connections_len != 0)
        {
            int64_t t = rnd % connections_len;
            log("Want run remote, but: %lld %lld [con=%p]\n", connections[t]->wait_list_len, connections[t]->queue_len, connections[t]);
            if (connections[t]->outgoing != INVALID_SOCKET &&
                ((connections[t]->wait_list_len < 50 && connections[t]->queue_len < 30) ||
                  connections[t]->queue_len == 0))
            {
                StartNewWorkerRemote(connections[t], workerId, (IS_CALL_PARAM_GLOBAL_ID(global_id) ? global_id : 0), inputTable);
                ReleaseSRWLockShared(&connections_lock);
                return;
            }
        }
        ReleaseSRWLockShared(&connections_lock);
    }

    log("Starting new local worker %lld [input table %p]\n", workerId, inputTable);

    // TODO: remove 2048 body size constant
    int64_t tableSize = Workers[workerId].inputSize;
    void *data = myMalloc(1024 + 2048);
    memcpy(data + 1024 - tableSize, inputTable, tableSize);

    struct thread_data* lc_data = TlsGetValue(dwTlsIndex);
    
    struct queued_worker *t = myMalloc(sizeof(*t));
    t->id = workerId;
    t->depth = (lc_data ? lc_data->runningDepth + 1 : 0);
    t->data = Workers[workerId].data;
    t->rdiValue = (int64_t)data + 1024 - tableSize;
    t->rbpValue = (BYTE *)data + 1024;
    memset(t->context, 0, sizeof(t->context));

    queue_enqueue(t);
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
void *LoadWorker(BYTE *file, int64_t fileLength, int64_t *res_len, int64_t *ProcessEntryId)
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
        ////// Possible header types:
        //<<--Quote-->> from:../../../codegen/codegen.hpp:.*GetHeaderId.*\n?\{(?>[^{}]+|(?<o>\{)|(?<-o>\}))+(?(o)(?!))\}
        // static inline int8_t GetHeaderId(enum header_id_action action, const string &provider="")
        // {
        //     switch (action)
        //     {
        //         case ACTION_NEW_OBJECT:
        //             return (provider == "x64" ? 2 : 22);
        //         case ACTION_PUSH_OBJECT:
        //             return (provider == "x64" ? 0 : 20);
        //         case ACTION_QUERY_OBJECT:
        //             return (provider == "x64" ? 1 : 21);
        //         case ACTION_PUSH_PIPE:
        //             return (provider == "x64" ? 8 : 28);
        //         case ACTION_QUERY_PIPE:
        //             return (provider == "x64" ? 9 : 29);
        //         case ACTION_CALL_WORKER:
        //             return (provider == "x64" ? 3 : 23);
        //         case ACTION_CAST_PROVIDER:
        //             return 10;
        //         case HEADER_DLL_IMPORT:
        //             return 4;
        //         case HEADER_X64_WORKERS:
        //             return 16;
        //         case HEADER_GPU_WORKERS:
        //             return 18;
        //         case HEADER_STRINGS_TABLE:
        //             return 17;
        //     }
        // }
        //<<--QuoteEnd-->>
        BYTE type = *pos++;
        switch (type)
        {
            case 0:
            case 20:
            case 1:
            case 21:
            case 2:
            case 22:
            case 42:
            case 3:
            case 23:
            case 33:
            case 8:
            case 28:
            case 9:
            case 29:
            case 10:
            {
                // read positions and replace calls
                int64_t count = *(int64_t *)pos;
                pos += 8;
                log("header %lld of size %lld\n", (int64_t)type, count);
                for (int64_t i = 0; i < count; ++i)
                {
                    log("set to %lld ", *(int64_t *)pos);
                    uint64_t *callPosition = (uint64_t *)(mem + *(int64_t *)pos);
                    pos += 8;
                    switch (type)
                    {
                        case 0:  *callPosition = (uint64_t)&x64_fastPushObject; break;
                        // case 20: *callPosition = (uint64_t)&gpu_fastPushObject; break;
                        case 1:  *callPosition = (uint64_t)&x64_fastQueryObject; break;
                        // case 21: *callPosition = (uint64_t)&gpu_fastQueryObject; break;
                        case 2:  *callPosition = (uint64_t)&x64_fastNewObject; break;
                        case 22: *callPosition = (uint64_t)&gpu_fastNewObject; break;
                        case 42: *callPosition = (uint64_t)&loc_fastNewObject; break;
                        case 3:  *callPosition = (uint64_t)&x64_fastCallObject; break;
                        case 23: *callPosition = (uint64_t)&gpu_fastCallObject; break;
                        case 33: *callPosition = (uint64_t)&dll_fastCallObject; break;
                        case 8:  *callPosition = (uint64_t)&x64_fastPushPipe; break;
                        // case 28: *callPosition = (uint64_t)&gpu_fastPushPipe; break;
                        case 9:  *callPosition = (uint64_t)&x64_fastQueryPipe; break;
                        // case 29: *callPosition = (uint64_t)&gpu_fastQueryPipe; break;
                        case 10:  *callPosition = (uint64_t)&any_fastCastProvider; break;
                        default:
                            print("ERROR: Runtime endpoint %lld doensn't supported for now [gpu push/query]\n");
                            ExitProcess(1);
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
                int64_t inputs_len = *(int64_t *)pos;
                pos += 8;
                struct dll_input_table *inputs = myMalloc(sizeof(*inputs) * inputs_len);
                for (int64_t i = 0; i < inputs_len; ++i)
                {
                    inputs[i].type = *pos++;
                    inputs[i].size = *(int64_t *)pos;
                    pos += 8;
                    inputs[i].param = *(int64_t *)pos;
                    pos += 8;
                    totalSize += inputs[i].size;
                }
                // set information
                struct dll_worker_info *data = myMalloc(sizeof(*data));

                // int64_t num_chars = strlen(lib_name) + 1;
                // wchar_t *utf16 = myMalloc(sizeof(*utf16) * num_chars);
                // MultiByteToWideChar(CP_UTF8, 0, lib_name, -1, utf16, num_chars);

                HINSTANCE lib = LoadLibraryA(lib_name);
                data->entry = GetProcAddress(lib, entry);
                data->output_size = output_size;
                data->inputMapLength = inputs_len;
                data->inputMap = inputs;
                data->call_stack_usage = 32 + 16 * (inputs_len < 4 ? 0 : (inputs_len - 4 + 1) / 2);
                Workers[id] = (struct worker_info){PROVIDER_DLL, data, totalSize};

                // log data
                log("worker %lld is dll call of library %s %s -> result function is %p\n", id, lib_name, entry, data->entry);
                log("stack usage: %lld\n", data->call_stack_usage);
                log("output have size %lld [and args of total size %lld]\n", output_size, totalSize);
                for (int64_t i = 0; i < inputs_len; ++i)
                {
                    log("argument %lld have size %lld [type %02x]\n", i, inputs[i].size, inputs[i].type);
                }
                break;
            case 16: // x64 Worker positions
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
                    Workers[id] = (struct worker_info){PROVIDER_X64, ptr, tableSize};
                    log("Worker %lld [x64] have been loaded to %p [offset %llx] with input table of size %lld\n", id, ptr, offset, tableSize);
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
                            print("READ STRING:\n");
                            myPrintf((wchar_t *)defined_arrays[i].start);
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
            case 18: // gpu Worker positions
            {
                int64_t count = *(int64_t *)pos;
                pos += 8;
                for (int64_t i = 0; i < count; ++i)
                {
                    // read id, position and input table size
                    int64_t id = *(int64_t *)pos;
                    pos += 8;
                    int64_t start = *(int64_t *)pos;
                    pos += 8;
                    int64_t end = *(int64_t *)pos;
                    pos += 8;
                    int64_t tableSize = *(int64_t *)pos;
                    pos += 8;
                    int64_t inputMapLength = *(int64_t *)pos;
                    pos += 8;
                    struct gpu_input_table *map = myMalloc(sizeof(*map) * inputMapLength);
                    for (int64_t j = 0; j < inputMapLength; ++j)
                    {
                        map[j].size = *(int64_t *)pos;
                        pos += 8;
                        map[j].type = *pos++;
                    }
                    // set data
                    struct gpu_worker_info *info = myMalloc(sizeof(*info));
                    info->start = mem + start;
                    info->end = mem + end;
                    info->kernel_lock = (SRWLOCK)SRWLOCK_INIT;
                    info->inputMapLength = inputMapLength;
                    info->inputMap = map;
                    // build kernel
                    int err;
                    print("building kernel...\n");
                    BYTE tmp = *info->end;
                    *info->end = 0;
                    print("%s\n", info->start);
                    *info->end = tmp;
                    info->kernel = gpuBuildFromText(SL_main_platform, 0, "krnl", info->start, info->end - info->start, &err);
                    if (err != 0)
                    {
                        print("Error: kernel build failed\n");
                        ExitProcess(1);
                    }
                    Workers[id] = (struct worker_info){PROVIDER_GPU, info, tableSize};
                    log("Worker %lld [GPU] have been loaded to %p [offset %llx:%llx] with input table of size %lld\n", id, info, start, end, tableSize);
                }
                break;
            }
            case 80: // entry ID
            {
                int64_t entryId = *(int64_t *)pos;
                pos += 8;
                *ProcessEntryId = entryId;
                break;
            }
            default:
                log("Error: unknown header type %lld\n", (int64_t)type);
                break;
        }
    }
    return mem;
}


struct sheduler_instance_info
{
    int64_t number;
    int64_t resCodeId;
};


DWORD ShedulerInstance(void *vparam)
{
    struct sheduler_instance_info *param = vparam;
    struct thread_data *lc_data = myMalloc(sizeof(*lc_data));
    int64_t resCodeId = param->resCodeId;
    TlsSetValue(dwTlsIndex, lc_data);
    lc_data->number = (int64_t)param->number;
    lc_data->completedTasks = 0;
    lc_data->prevPrint = 0;
    while (1)
    {
        SheduleWorker(lc_data);

        // if resCodeId is ready - print it and return
        struct object_promise *p = (void *)GetHashtable(&local_objects, (BYTE *)&resCodeId, 8, 0);
        if (p != NULL)
        {
            p = (void *)((BYTE *)p - DATA_OFFSET(*p));
            if (p->ready)
            {
                print("ShedulerInstance completed\n");
                return 0;
            }
            // print("promise not set\n");
        }
        else
        {
            print("promise not found\n");
        }
    }
    myFree(lc_data);
    return 0;
}


#ifdef FREESTANDING
int entry()
#else
int wmain(void)
#endif
{
    ////////////////////////// loading stage
    init_lib();
    if (init_gpu_subsystem())
    {
        print("Gpu sussystem initialization failed\n");
        return 1;
    }

    dwTlsIndex = TlsAlloc();


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
    int64_t entryWorker = 0;
    void *res = LoadWorker(buf, len, &res_len, &entryWorker);
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


    // cmdargs
    
    int NUM_THREADS = 1;
    int64_t inputLen = 0, connectingToMain = 0;
    int64_t resCodeId = 0, hangAfterEnd = 0, noStdin = 0;
    int argc;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    while (argc > 1)
    {
        if (argv[1][0] == 'n')
        {
            noStdin = 1;
        }
        else if (argv[1][0] == 'h')
        {
            hangAfterEnd = 1;
        }
        else if (argv[1][0] == 'j')
        {
            NUM_THREADS = argv[1][1] - '0';
        }
        else if (argv[1][0] == 'n')
        {
            connectingToMain = 1;
        }
        else if (argv[1][0] == '-' && argv[1][1] == '-')
        {
            break;
        }
        else
        {
            print("Unknown cmdline parameter: %lld\n");
            return 1;
        }
        argv++;
        argc--;
    }

    ////////////////////////// running stage

    InitInternalStructures();
    queue_init();
    start_remote_subsystem(noStdin);

    // run first worker with comand line arguments as i64 array    
    
    print("NUM_THREADS=%lld\n", NUM_THREADS);
    if (!connectingToMain)
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
        int64_t *input;
        if (noStdin)
        {
            inputLen = 1;
            input = myMalloc(8 * inputLen);
            *input = 0;
        }
        else
        {
            inputLen = myScanI64();
            input = myMalloc(8 * inputLen);
            for (int64_t i = 0; i < inputLen; ++i)
            {
                input[i] = myScanI64();
            }
        }
        #endif

        log("Entry worker id=%lld\n", entryWorker);

        resCodeId = StartInitialProcess(entryWorker, input, inputLen);

        log("result promise %p %lld\n", resCodeId);
    }

    print("Running...\n");

    // TODO: make better program end determination
    (void)connectingToMain;

    HANDLE hThreads[NUM_THREADS];
    DWORD threadId;

    for (int64_t i = 0; i < NUM_THREADS; ++i)
    {
        struct sheduler_instance_info *info = myMalloc(sizeof(*info));
        info->number = i;
        info->resCodeId = resCodeId;
        hThreads[i] = CreateThread(NULL, 0, ShedulerInstance, info, 0, &threadId);
        if (hThreads[i] == NULL)
        {
            print("Failed to create thread %lld\n", i);
            return 1;
        }
    }

    DWORD waitResult = WaitForMultipleObjects(NUM_THREADS, hThreads, TRUE, INFINITE);
    (void)waitResult;
    
    print("Program finished\n");

    TlsFree(dwTlsIndex);

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
            print("Program exited with code %llx\n", *(int *)p->data);

            if (hangAfterEnd)
            {
                print("Press Ctrl+C to end\n");
                while (1){};
            }

            ExitProcess(*(int *)p->data);
        }
        else
        {
            print("Result of main function isn't ready after program end\n");
        }
    }

    if (hangAfterEnd)
    {
        print("Press Ctrl+C to end\n");
        while (1){};
    }

    ExitProcess(1);
}
