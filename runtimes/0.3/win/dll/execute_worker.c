#define UNICODE 1
#define _UNICODE 1
#define _WIN32_WINNT _WIN32_WINNT_VISTA

#include "inttypes.h"
#include "dll.h"
#include "../providers.h"
#include "../runtime_api.h"
#include "../x64/x64.h"

struct dll_input_data {
    int64_t id;
    int64_t size;
};

struct dll_data
{
    int64_t current_index;
    int64_t waitedSize;
    void *inputTable;
    void *currentArg;
    void *output;
    struct dll_input_data inputData[];
};

#define RETURN_STATE_DLL_END ((void *)1)
#define RETURN_STATE_DLL_ARRAY_SIZE_REQUEST ((void *)2)
#define RETURN_STATE_DLL_ARGUMENT_VALUE ((void *)3)
#define RETURN_STATE_DLL_SET_RESULTS ((void *)4)

struct dll_call_data
{
    void *loaded_function;
    int64_t output_size;
    int64_t sizes_len;
    int64_t call_stack_usage;
};
extern int DllCall(struct dll_call_data *, int64_t *, void *);

void dllExecuteWorker(struct queued_worker *worker)
{
    if (Workers[worker->id].data == worker->data)
    {
        worker->data = NULL;
    }

    struct dll_data *data;
    struct dll_worker_info *info = Workers[worker->id].data;
    int64_t inputTableSize = Workers[worker->id].inputSize;

    switch ((int64_t)worker->data)
    {
    case (int64_t)NULL:
        // worker start
        // TODO: set interrupt lock
        log("RUN dllimport worker=%p\n", worker);
        // print("worker id=%lld [dll]\n", worker->id);

        {
            BYTE *inputTable = (BYTE *)worker->rdiValue;
            /* "rbpValue" */
            data = myMalloc(sizeof(*data) + sizeof(*data->inputData)*info->inputMapLength);
            memset(data, 0, sizeof(*data));
            data->currentArg = data->inputTable = myMalloc(inputTableSize);
            memcpy(data->inputTable, inputTable, inputTableSize);
        }

        data->current_index = 0;
        
        // collect all arguments to this worker
        while (data->current_index < info->inputMapLength)
        {
            int64_t provider = info->inputMap[data->current_index].provider;
            int64_t type = info->inputMap[data->current_index].type;
            int64_t size = info->inputMap[data->current_index].param;
            if ((type & 0xF) == 0)
            {
                int64_t obj = *(int64_t *)data->currentArg;
                
                if (obj != 0)
                {
                    if (provider == PROVIDER_X64)
                    {
                        // request size
                        data->waitedSize = 0;
                        x64QueryObject(&data->waitedSize, obj, -8, 6, RETURN_STATE_DLL_ARRAY_SIZE_REQUEST, data);

                        if (0) {
                    case (int64_t)RETURN_STATE_DLL_ARRAY_SIZE_REQUEST:
                            data = worker->rbpValue;
                            provider = info->inputMap[data->current_index].provider;
                            type = info->inputMap[data->current_index].type;
                        }
                        size = data->waitedSize;
                    }
                    else if (provider == PROVIDER_LOC)
                    {
                        data->waitedSize = size = ((struct object *)(obj - DATA_OFFSET(struct object)))->data_size;
                    }
                    else
                    {
                        print("Error: UNKNOWN provider %lld in DLL call\n", provider);
                        trap;
                    }
                }
            }

            // request data
            if ((type & 0xF) != 2)
            {
                int64_t obj = *(int64_t *)data->currentArg;
                log("Requesting %lld bytes\n", size);
                
                if (obj != 0)
                {
                    // create parameter pointer
                    data->inputData[data->current_index].id = obj;
                    data->inputData[data->current_index].size = size;
                    void *tmp;
                    *(void **)data->currentArg = tmp = myMalloc(size);
                    if (provider == PROVIDER_X64)
                    {
                        x64QueryObject(tmp, obj, 0, size, RETURN_STATE_DLL_ARGUMENT_VALUE, data);

                        if (0) {
                    case (int64_t)RETURN_STATE_DLL_ARGUMENT_VALUE:
                            data = worker->rbpValue;
                        }
                    }
                    else if (provider == PROVIDER_LOC)
                    {
                        memcpy(tmp, (void *)obj, size);
                    }
                    else
                    {
                        print("Error: UNKNOWN provider %lld in DLL call\n", provider);
                        trap;
                    }
                }
            }

            // move to next argument
            data->currentArg += info->inputMap[data->current_index].size;
            data->current_index++;
        }

        // now, print them
        {
            BYTE *a = data->inputTable;
            for (int64_t i = 0; i < info->inputMapLength; ++i)
            {
                log("Argument %lld: [%02x] [to->%p]\n", i, info->inputMap[i].type, data->inputData[i]);
                log("         of size %lld; =%p\n", info->inputMap[i].size, *(int64_t *)a);
                a += info->inputMap[i].size;
            }
        }

        // now, call dll worker
        // TODO: lock interrupts mutex
        // prepare data
        int64_t result_promise_id = 0;
        int64_t *call_data = __builtin_alloca(sizeof(int64_t) * (info->inputMapLength + 1)), *cd;
        cd = call_data;

        // if need space for output - allocate it
        data->output = 0;
        if (info->output_size != -1)
        {
            data->output = myMalloc(info->output_size);
            
            if (info->output_size != 1 &&
                info->output_size != 2 &&
                info->output_size != 4 &&
                info->output_size != 8)
            {
                *cd++ = (int64_t)data->output;
            }
        }
        
        BYTE *args = data->inputTable;
        for (int64_t i = 0; i < info->inputMapLength; ++i)
        {
            switch (info->inputMap[i].size)
            {
                case 1:
                case 2:
                case 4:
                case 8:
                {
                    *cd = 0;
                    memcpy(cd++, args, info->inputMap[i].size);
                    break;
                }
                default:
                {
                    *cd++ = (int64_t)args;
                    break;
                }
            }
            args += info->inputMap[i].size;
        }

        if (info->output_size != -1)
        {
            result_promise_id = *(int64_t*)args;
            args += 8;
        }

        log("calling dllimport worker\n");
        for (int64_t i = 0; i < info->inputMapLength; ++i)
        {
            log("ARG[%lld] = %lld\n", i, call_data[i]);
        }
        log("output is %p [->to promise %lld]\n", data->output, result_promise_id);

        struct dll_call_data cl_data = {
            info->entry,
            info->output_size,
            info->inputMapLength,
            info->call_stack_usage
        };

        int64_t prevError = GetLastError();
        
        DllCall(&cl_data, call_data, data->output);

        if (GetLastError() != 0 && prevError != GetLastError())
        {
            print("returned + Error=%lld [before call error was %lld] [entry=%s]\n", (int64_t)GetLastError(), prevError, info->entryName);
            trap;
        }
        
        // for all output parameters: set them back        
        data->current_index = 0;
        data->currentArg = data->inputTable;
        while (data->current_index < info->inputMapLength)
        {
            int64_t type = info->inputMap[data->current_index].type;
            int64_t provider = info->inputMap[data->current_index].provider;
            if (type & 0x10 && data->inputData[data->current_index].id)
            {
                if (provider == PROVIDER_X64)
                {
                    int64_t size = data->inputData[data->current_index].size;
                    void *mem = *(void **)data->currentArg;
                    // set it back, to object
                    x64PushObject(data->inputData[data->current_index].id, mem, 0, size, RETURN_STATE_DLL_SET_RESULTS, data);

                    if (0) {
                case (int64_t)RETURN_STATE_DLL_SET_RESULTS:
                        data = worker->rbpValue;
                        mem = *(void **)data->currentArg;
                    }
                }
                else if (provider == PROVIDER_LOC)
                {
                    int64_t size = data->inputData[data->current_index].size;
                    void *mem = *(void **)data->currentArg;
                    memcpy((void *)data->inputData[data->current_index].id, mem, size);
                }
                else
                {
                    print("Error: UNKNOWN provider %lld in DLL call\n", provider);
                    trap;
                }
            }
            if (data->inputData[data->current_index].id)
            {
                void *mem = *(void **)data->currentArg;
                myFree(mem);
            }
            // move to next argument
            data->currentArg += info->inputMap[data->current_index].size;
            data->current_index++;
        }
        
        if (info->output_size != -1)
        {
            result_promise_id = *(int64_t*)(data->inputTable + Workers[worker->id].inputSize - 8);
            x64PushObject(result_promise_id, data->output, 0, info->output_size, RETURN_STATE_DLL_END, data);
        }

        if (0)
        {
    case (int64_t)RETURN_STATE_DLL_END:
            data = worker->rbpValue; 
        }
        
        // free all allocated memroy
        if (data->output)
        {
            // myFree(data->output);
            data->output = 0;
        }
        myFree(data->inputTable);
        myFree(data);
        return;
    }
}
