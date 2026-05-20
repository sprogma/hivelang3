#include "system.h"

#include "inttypes.h"

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

int printStats = 1;

int64_t NUM_THREADS = 1;
int64_t CHUNK_TIME_US = 50000;
struct defined_array *defined_arrays;

struct worker_info Workers[100] = {};
struct hive_provider_info Providers[] = {
    {
        .ExecuteWorker = x64ExecuteWorker,
        .NewObjectUsingPage = x64NewObjectUsingPage,
        .FreeWaitingWorker = x64FreeWaitingWorker,
        .stallable = 1,
        .TryStallWorker = x64TryStallWorker,
        .StartNewLocalWorker = x64StartNewLocalWorker,
    },
    {
        .ExecuteWorker = gpuExecuteWorker,
        .NewObjectUsingPage = gpuNewObjectUsingPage,
        .FreeWaitingWorker = NULL,
        .stallable = 0,
        .StartNewLocalWorker = gpuStartNewLocalWorker,
    },
    {
        .ExecuteWorker = dllExecuteWorker,
        .NewObjectUsingPage = NULL,
        .FreeWaitingWorker = dllFreeWaitingWorker,
        .stallable = 0,
        .StartNewLocalWorker = dllStartNewLocalWorker,
    },
    {
        .ExecuteWorker = NULL,
        .NewObjectUsingPage = locNewObjectUsingPage,
        .FreeWaitingWorker = NULL,
        .stallable = 0,
        .StartNewLocalWorker = NULL,
    }
};

tls_index_t dwTlsIndex;

struct wait_list_node * _Atomic wait_list = NULL;
_Atomic int64_t wait_list_len = 0;

void RegisterObjectWithId(int64_t id, void *object)
{
    int64_t result = i64SetHashtable(&local_objects, id, (int64_t)object, 0);
    assert(result == 0);
}

int64_t GetNewObjectId(int64_t *result)
{
    int64_t remote_id = 0, set = 0;
    lock_read(&pages_lock);
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
    unlock_read(&pages_lock);

    *result = remote_id;
    return set;
}


struct waiting_worker *universalPauseWorker(void *returnAddress, void *rbpValue, enum worker_wait_state state, void *state_data)
{
    struct thread_data* lc_data = TlsGetValue(dwTlsIndex);
    switch (Workers[lc_data->runningId].provider)
    {
        case PROVIDER_X64:
            return x64PauseWorker(returnAddress, rbpValue, state, state_data);
    }
    print("Error: this worker doen't support universal pause\n");    
    ExitProcess(1);
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


void EnqueueWorkerFromWaitList(struct waiting_worker *w, int64_t rdi_value)
{
    int64_t old = 0;
    if (atomic_compare_exchange_strong(&w->queued, &old, 1)) // to lock it only once
    {
        struct queued_worker *t = AllocateQueuedWorker();
        t->id = w->id;
        t->depth = w->depth;
        t->data = w->data;
        t->rbpValue = w->rbpValue;
        t->rdiValue = rdi_value;
        memcpy(t->context, w->context, sizeof(t->context));
        log("Worker enqueued [id=%lld, data=%p]\n", t->id, t->data);
        struct thread_data* lc_data = TlsGetValue(dwTlsIndex);
        int64_t number = 0;
        if (lc_data != NULL)
        {
            number = lc_data->number;
        }
        scheduler_enqueue(&glb_scheduler, 1, t, number);
    }
}

int64_t UpdateSingleWorker(int64_t ticks, struct waiting_worker *w)
{
    int64_t res = 0;
    int64_t rdiValue = 0;
    // log("worker [data=%p]: wait for %lld\n", w, w->state);
    switch (w->state)
    {
        // declarations
        //<<--Quote-->> from::(ls *.c -r|sls "^\s*//@reg\s+(\w+)\s+(\w+)$"|% Matches|%{[pscustomobject]@{a=$_.Groups[1];b=$_.Groups[2]}}|group b|%{$n=$_;$_.Group|%{"$(" "*12)int64_t $($n.Name)(struct waiting_worker *, int64_t, int64_t *);"}}|s -u)-join"`n"
        int64_t anyCastStates(struct waiting_worker *, int64_t, int64_t *);
        int64_t anyCastStates(struct waiting_worker *, int64_t, int64_t *);
        int64_t anyCastStates(struct waiting_worker *, int64_t, int64_t *);
        int64_t anyCastStates(struct waiting_worker *, int64_t, int64_t *);
        int64_t anyCastStates(struct waiting_worker *, int64_t, int64_t *);
        int64_t x64NewObjectStates(struct waiting_worker *, int64_t, int64_t *);
        int64_t x64PushObjectStates(struct waiting_worker *, int64_t, int64_t *);
        int64_t x64QueryObjectStates(struct waiting_worker *, int64_t, int64_t *);
        int64_t x64SleepStates(struct waiting_worker *, int64_t, int64_t *);
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
        case WK_STATE_TIMER_WAIT_X64:
            res = x64SleepStates(w, ticks, &rdiValue); break;
        //<<--QuoteEnd-->>
    }
       
    if (res)
    {
        EnqueueWorkerFromWaitList(w, rdiValue);

        int64_t tmp = atomic_fetch_sub(&w->links, 1);
        assert(tmp >= 1);
        if (tmp == 1)
        {
            FreeWaitingWorker(w);
        }
    }

    return res;
}

void UpdateWaitingQueryWorkers(int64_t ticks)
{
    struct hashtable *h = atomic_load(&get_wait_list);
    while (h)
    {
        for (int i = 0; i < h->alloc; ++i)
        {
            if (h->table[i].key_ptr != NULL)
            {
                log("looping... [i=%lld/%lld]", (int64_t)i, h->alloc);
                log("taking...");
                int64_t value = TakeTaggedHashtable(&get_wait_list, h->table[i].key_ptr, GETSET_WAIT_LIST_VALUE_PROCESSING_TAG, GETSET_WAIT_LIST_PARALLEL_PROCESSING);
                log("done.");
                struct get_wait_list_value *q = (void *)value;
                // ! is q is null, no tag will be created

                // now, q is top pointer, and hashtable is tagged
                if (q != NULL)
                {
                    log("Upadting waiting query worker...\n");
                    if (q->callback == callbackContinueWorkerFromWaitingQuery)
                    {
                        struct waiting_worker *w = q->params;
                        UpdateSingleWorker(ticks, w);
                    }
                    // and now, release tag
                    AddHashtable(&get_wait_list, h->table[i].key_ptr, -1);
                    log("released.");
                }
            }
        }
        h = h->prev;
    }
}

void UpdateWaitingPushWorkers(int64_t ticks)
{
    struct hashtable *h = atomic_load(&set_wait_list);
    while (h)
    {
        for (int i = 0; i < h->alloc; ++i)
        {
            if (h->table[i].key_ptr != NULL)
            {
                log("looping... [i=%lld/%lld]", (int64_t)i, h->alloc);
                log("taking...", (int64_t)i, h->alloc);
                int64_t value = TakeTaggedHashtable(&set_wait_list, h->table[i].key_ptr, GETSET_WAIT_LIST_VALUE_PROCESSING_TAG, GETSET_WAIT_LIST_PARALLEL_PROCESSING);
                log("done.");
                struct set_wait_list_value *q = (void *)value;
                // ! is q is null, no tag will be created

                // now, q is top pointer, and hashtable is tagged
                if (q != NULL)
                {
                    log("Upadting waiting push worker...\n");
                    if (q->callback == callbackContinueWorkerFromWaitingPush)
                    {
                        struct waiting_worker *w = q->params;
                        UpdateSingleWorker(ticks, w);
                    }
                    // and now, release tag
                    AddHashtable(&set_wait_list, h->table[i].key_ptr, -1);
                    log("released.");
                }
            }
        }
        h = h->prev;
    }
}

void UpdateWaitingWorkers(int full_scan)
{
    int64_t ticks = GetTicks();
    int count = 0;
    const int THRESHOLD = 16 * 1024; 
        
    // log("Update waiting workers\n");

    // update waiting for requests workers...
    if (full_scan)
    {
        if ((ticks & 0xFF) < 30)
        {
            // log("start A\n");
            UpdateWaitingQueryWorkers(ticks);
        }
        if (((ticks + 179) & 0xFF) < 30)
        {
            // log("start B\n");
            UpdateWaitingPushWorkers(ticks);
        }
        
        // log("2/3 completed\n");
    }
    
    // take all workers
    struct wait_list_node *data = NULL;
    data = atomic_exchange(&wait_list, data);
    wait_list_len = 0; // this can be not right, but it is ok

    // work with them:
    for (struct wait_list_node *curr = data, *nxt = data ? data->next : data; curr; curr = nxt, nxt = nxt ? nxt->next : nxt)
    {
        struct waiting_worker *w = curr->worker;

        count += UpdateSingleWorker(ticks, w);        
        
        if (count >= THRESHOLD && nxt) 
        {
            int64_t cnt = 1;
            struct wait_list_node *tail = nxt;
            while (tail->next) { tail = tail->next; cnt++; };

            log("returning %lld workers to wait list\n", cnt);

            struct wait_list_node *old_head = atomic_load(&wait_list);
            do {
                tail->next = old_head;
            } while (!atomic_compare_exchange_weak(&wait_list, &old_head, nxt));

            atomic_fetch_add(&wait_list_len, cnt);

            log("got %lld workers there\n", wait_list_len);
            
            return;
        }
                
        FreeWaitingNode(curr);
    }
    
    // log("3/3 completed.\n");
}

void StartNewWorker(int64_t workerId, int64_t global_id, BYTE *inputTable)
{
    /* if we are running too many tasks - redirect new worker to another hive */
    log("Run worker %lld on requested = %lld\n", workerId, global_id);
    int64_t rnd = 0;
    SECURE_RANDOM(&rnd, 8);
    rnd = myAbs(rnd);
    int64_t random_confirm = (rnd & 0x80000000) && (rnd % 100 < 5 * (int64_t)(wait_list_len + glb_scheduler.len));
    if (((random_confirm || global_id != 0) && global_id != 1) || global_id == 2)
    {
        /* select random connection */
        lock_read(&connections_lock);
        if (connections_len != 0)
        {
            int64_t t = rnd % connections_len;
            log("Want run remote, but: %lld %lld [con=%p]\n", connections[t]->wait_list_len, connections[t]->queue_len, connections[t]);
            if (connections[t]->outgoing.sock != INVALID_SOCKET &&
                ((connections[t]->wait_list_len < 50 && connections[t]->queue_len < 30) ||
                  connections[t]->queue_len == 0 || 
                    global_id == 2))
            {
                StartNewWorkerRemote(connections[t], workerId, (IS_CALL_PARAM_GLOBAL_ID(global_id) ? global_id : 0), inputTable);
                unlock_read(&connections_lock);
                return;
            }
        }
        else if (global_id == 2)
        {
            print("Error: can't run remote worker %lld: no remote connections found\n", workerId);
        }
        unlock_read(&connections_lock);
    }

    log("Starting new local worker %lld [input table %p]\n", workerId, inputTable);
    if (Providers[Workers[workerId].provider].StartNewLocalWorker)
    {
        Providers[Workers[workerId].provider].StartNewLocalWorker(workerId, inputTable);
    }
    else
    {
        print("Error: provider %lld doesn't support StartNewLocalWorker\n", Workers[workerId].provider);
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
    #ifdef _WIN32
    void *mem = VirtualAlloc(NULL, fileLength - codePosition, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    #else
    void* mem = mmap(NULL, fileLength - codePosition, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    #endif
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
        //         case ACTION_SLEEP:
        //             if (provider == "x64") return 50;
        //             break;
        //         case ACTION_NEW_OBJECT:
        //             if (provider == "x64") return 2;
        //             if (provider == "gpu") return 22;
        //             if (provider == "loc") return 42;
        //             break;
        //         case ACTION_PUSH_OBJECT:
        //             if (provider == "x64") return 0;
        //             if (provider == "gpu") return 20;
        //             break;
        //         case ACTION_QUERY_OBJECT:
        //             if (provider == "x64") return 1;
        //             if (provider == "gpu") return 21;
        //             break;
        //         case ACTION_PUSH_PIPE:
        //             return (provider == "x64" ? 8 : 28);
        //         case ACTION_QUERY_PIPE:
        //             return (provider == "x64" ? 9 : 29);
        //         case ACTION_CALL_WORKER:
        //             if (provider == "x64") return 3;
        //             if (provider == "gpu") return 23;
        //             if (provider == "dll") return 33;
        //             break;
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
        //         case HEADER_ENTRY_ID:
        //             return 80;
        //     }
        //     printf("Error: unsupported action: %lld on provider %s\n", (int64_t)action, provider.c_str());
        //     return -1;
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
            case 50:
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
                        case 50:  *callPosition = (uint64_t)&x64_fastSleep; break;
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
                int64_t entryLen = 0;
                {
                    int64_t sz = *(int64_t *)pos;
                    pos += 8;
                    memcpy(entry, pos, sz);
                    entry[sz] = 0;
                    pos += sz;
                    entryLen = sz;
                }
                // read argument sizes
                int64_t affinity = *(int64_t *)pos;
                pos += 8;
                int64_t totalSize = 8;
                int64_t output_size = *(int64_t *)pos;
                pos += 8;
                int64_t inputs_len = *(int64_t *)pos;
                pos += 8;
                struct dll_input_table *inputs = myMalloc(sizeof(*inputs) * inputs_len);
                for (int64_t i = 0; i < inputs_len; ++i)
                {
                    inputs[i].type = *pos++;
                    inputs[i].provider = *(int64_t *)pos;
                    pos += 8;
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

                #ifdef _WIN32
                HINSTANCE lib = LoadLibraryA(lib_name);
                data->entry = GetProcAddress(lib, entry);
                data->entryName = myMalloc(entryLen+1);
                data->output_size = output_size;
                data->inputMapLength = inputs_len;
                data->inputMap = inputs;
                data->call_stack_usage = 32 + 16 * (inputs_len < 4 ? 0 : (inputs_len - 4 + 1) / 2);
                memcpy(data->entryName, entry, entryLen+1);
                Workers[id] = (struct worker_info){PROVIDER_DLL, data, totalSize, affinity};
                #else
                void *lib = dlopen(lib_name, RTLD_LAZY);
                data->entry = dlsym(lib, entry);
                data->entryName = myMalloc(entryLen + 1);
                data->output_size = output_size;
                data->inputMapLength = inputs_len;
                data->inputMap = inputs;

                data->call_stack_usage = 0;
                for (int64_t i = 0, count = 6; i < inputs_len; ++i)
                {
                    if (inputs[i].size > 16)
                    {
                        data->call_stack_usage += (inputs[i].size + 7) / 8 * 8;
                        count = 0;
                        continue;
                    }
                    if (inputs[i].size <= 8 * count)
                    {
                        count -= (inputs[i].size + 7) / 8;
                        continue;
                    }
                    data->call_stack_usage += (inputs[i].size + 7) / 8 * 8;
                }
                data->call_stack_usage = (data->call_stack_usage + 15) / 16 * 16;

                memcpy(data->entryName, entry, entryLen + 1);
                Workers[id] = (struct worker_info){PROVIDER_DLL, data, totalSize, affinity};
                #endif

                if (data->entry == NULL)
                {
                    print("Error: can't load dll function <%s> from library <%s>\n", entry, lib_name);
                    ExitProcess(1);
                }
                
                // log data
                log("worker %lld is dll call of library %s %s -> result function is %p\n", id, lib_name, entry, data->entry);
                log("stack usage: %lld\n", data->call_stack_usage);
                log("output have size %lld [and args of total size %lld]\n", output_size, totalSize);
                for (int64_t i = 0; i < inputs_len; ++i)
                {
                    log("argument %lld have size %lld [type %02x]\n", i, inputs[i].size, inputs[i].type);
                }
                // set gdi!
                #ifdef _WIN32
                GdiSetBatchLimit(1);
                #endif
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
                    int64_t size = *(int64_t *)pos;
                    pos += 8;
                    int64_t tableSize = *(int64_t *)pos;
                    pos += 8;
                    int64_t affinity = *(int64_t *)pos;
                    pos += 8;
                    // set data
                    void *ptr = mem + offset;
                    struct x64_worker_data *info = myMalloc(sizeof(*info));
                    *info = (struct x64_worker_data){
                        .start = ptr,
                        .end = ptr + size,
                        .nextBuffer = NULL,
                        .spinlock = 0,
                    };
                    Workers[id] = (struct worker_info){PROVIDER_X64, info, tableSize, affinity};
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
                    info->kernel_lock = (lock_t)INIT_LOCK;
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
                    Workers[id] = (struct worker_info){PROVIDER_GPU, info, tableSize, -1};
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



#ifdef FREESTANDING
int entry()
#else
#ifdef _WIN32
int wmain(void)
#else
int main(int argc, char **argv)
#endif
#endif
{
    log("starting...\n");
    ////////////////////////// loading stage
    init_lib();
    if (init_gpu_subsystem())
    {
        print("WARING: Gpu sussystem initialization failed\n");
    }

    #ifdef _WIN32
    dwTlsIndex = TlsAlloc();
    #else
    pthread_key_create(&dwTlsIndex, NULL);
    #endif

    #ifdef _WIN32
    HANDLE hFile = CreateFileW(
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
    #else
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wdeprecated-octal-literals"
    int fd = open("../../../res.bin", O_RDONLY);
    #pragma clang diagnostic pop
    if (fd == -1) {
        print("Error: can't open ../../../res.bin file\n");
        return 1;
    }
    struct stat st;
    if (fstat(fd, &st) == -1) {
        close(fd);
        return 1;
    }
    int64_t len = st.st_size;
    uint8_t *buf = (uint8_t*)myMalloc(len);
    ssize_t bytesRead = read(fd, buf, len);
    if (bytesRead == -1) {
        print("Error: can't open ../../../res.bin file\n");
        return 1;
    }
    close(fd);
    #endif

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
    
    int64_t inputLen = 0, connectingToMain = 0, localInput = 0;
    int64_t resCodeId = 0, hangAfterEnd = 0, noStdin = 0;
    #ifdef _WIN32
    int argc;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    #endif
    while (argc > 1)
    {
        if (argv[1][0] == 'n')
        {
            noStdin = 1;
            print("This hive will not read any stdin\n");
        }
        else if (argv[1][0] == 'h')
        {
            hangAfterEnd = 1;
            print("This hive will not stop execution\n");
        }
        else if (argv[1][0] == 'l')
        {
            localInput = 1;
            print("Input will be local array\n");
        }
        else if (argv[1][0] == 'q')
        {
            printStats = 0;
            print("Will not print table with information\n");
        }
        else if (argv[1][0] == 'j')
        {
            NUM_THREADS = 0;
            int64_t x = 1;
            while (argv[1][x])
            {
                NUM_THREADS *= 10;
                NUM_THREADS += argv[1][x] - '0';
                x++;
            }
            print("Set num threads = %lld\n", NUM_THREADS);
            if (NUM_THREADS > 64)
            {
                print("Error: this build doesn't allow more than 64 threads for PC safety\n");
                return 1;
            }
        }
        else if (argv[1][0] == 'c')
        {
            connectingToMain = 1;
            print("Set hive as protectorate\n", CHUNK_TIME_US);
        }
        else if (argv[1][0] == 'p')
        {
            CHUNK_TIME_US = 0;
            int64_t x = 1;
            while (argv[1][x])
            {
                CHUNK_TIME_US *= 10;
                CHUNK_TIME_US += argv[1][x] - '0';
                x++;
            }
            print("Set chunk time to %lld us\n", CHUNK_TIME_US);
        }
        else if (argv[1][0] == '-' && argv[1][1] == '-')
        {
            break;
        }
        else
        {
            print("Unknown cmdline parameter: %lld\n", argv[1][0]);
            return 1;
        }
        argv++;
        argc--;
    }

    ////////////////////////// running stage

    InitInternalStructures();
    scheduler_init(&glb_scheduler, NUM_THREADS);
    start_remote_subsystem(noStdin);

    // run first worker with comand line arguments as i64 array    
    
    print("NUM_THREADS=%lld\n", NUM_THREADS);
    if (!connectingToMain)
    {
        #ifdef _DEBUG
        inputLen = ((int64_t)argc - 2 < 0 ? 0 : (int64_t)argc - 2);
        int64_t *input = myMalloc(8 * inputLen);
        log("READING INPUT AS: ");
        for (int i = 2; i < argc; ++i)
        {
            input[i - 2] = myAtoll(argv[i]);
            log("%lld ", input[i - 2]);
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
            /* read all stdin */
            // ------------------------------------------------ TODO: rewrite input with more clean and fast methods
            #ifdef _WIN32
            HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
            const DWORD BUF_SIZE = 1024 * 1024; 
            char* buf = (char*)myMalloc(BUF_SIZE);
            DWORD bytesInBuf = 0;
            DWORD pos = 0;

            for (int64_t i = 0; i < inputLen; ++i) {
                int64_t val = 0;
                char c;
                while (1) {
                    if (pos >= bytesInBuf) {
                        if (!ReadFile(hStdin, buf, BUF_SIZE, &bytesInBuf, NULL) || bytesInBuf == 0) {
                            c = -1; break;
                        }
                        pos = 0;
                    }
                    c = buf[pos++];
                    if (c >= '0' && c <= '9') break; 
                    if (c == '-') break;             
                }
                int isNeg = 0;
                if (c == '-') { isNeg = 1; c = '0'; } 
                while (c >= '0' && c <= '9') {
                    val = val * 10 + (c - '0');
                    if (pos >= bytesInBuf) {
                        if (!ReadFile(hStdin, buf, BUF_SIZE, &bytesInBuf, NULL) || bytesInBuf == 0) {
                            c = -1; break;
                        }
                        pos = 0;
                    }
                    c = buf[pos++];
                }
                input[i] = isNeg ? -val : val;
            }
            #else
            for (int64_t i = 0; i < inputLen; ++i)
            {
                input[i] = myScanI64();
            }
            #endif
            myFree(buf);
        }
        #endif

        log("Entry worker id=%lld\n", entryWorker);

        resCodeId = StartInitialProcess(entryWorker, input, inputLen, localInput);

        log("result promise %p %lld\n", resCodeId);
    }

    print("Running...\n");
    if (connectingToMain)
    {
        print("-------------------------------------------------->>>>>>>>> this is client server\n");
    }
    else
    {
        print("-------------------------------------------------->>>>>>>>> this is main server\n");
    }

    // TODO: make better program end determination

    int64_t starttime = GetTicks();

    ShedulerStart(resCodeId);
    
    int64_t endtime = GetTicks();
    
    print("Program finished in %lld ms\n", TicksToMicroseconds(endtime - starttime) / 1000);

    #ifdef _WIN32
    TlsFree(dwTlsIndex);
    #else
    pthread_key_delete(dwTlsIndex);
    #endif

    struct object_promise *p = (void *)i64GetHashtable(&local_objects, resCodeId);
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

