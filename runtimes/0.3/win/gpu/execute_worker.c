#define UNICODE 1
#define _UNICODE 1
#define _WIN32_WINNT _WIN32_WINNT_VISTA

#include "inttypes.h"
#include "gpu.h"
#include "../providers.h"
#include "../gpu_subsystem.h"
#include "../x64/x64.h"

enum argumentType
{
    ARGUMENT_INDEX = 0x10,
    ARGUMENT_SIZE = 0x20,
    ARGUMENT_OFFSET = 0x30
};

#define MAX_DIMENSIONS 4

void gpuExecuteWorker(struct queued_worker *worker)
{
    if (worker->data == RETURN_STATE_GPU_END)
    {
        return;
    }
    // TODO: set interrupt lock
    myPrintf(L"RUN GPU worker=%p\n", worker);
    
    BYTE *inputTable = (BYTE *)worker->rdiValue;
    struct gpu_worker_info *info = Workers[worker->id].data;
    int64_t inputTableSize = Workers[worker->id].inputSize;
    int err;

    AcquireSRWLockExclusive(&info->kernel_lock);

    cl_uint dims = 0;
    size_t global_offset[MAX_DIMENSIONS];
    size_t global_size[MAX_DIMENSIONS];
    
    // set kernel arguments
    BYTE *currentArg = inputTable;
    for (int64_t i = 0, arg = 0; i < info->inputMapLength; ++i)
    {
        int64_t type = info->inputMap[i].type;
        if (type != 0)
        {
            if ((type & 0xF0) == ARGUMENT_INDEX)
            {
                // nothing to do
            }
            else if ((type & 0xF0) == ARGUMENT_SIZE)
            {
                dims = dims < (type & 0xF) + 1 ? (type & 0xF) + 1 : dims;
                global_size[type & 0xF] = *(int64_t *)currentArg;
            }
            else if ((type & 0xF0) == ARGUMENT_OFFSET)
            {
                global_offset[type & 0xF] = *(int64_t *)currentArg;
            }
        }
        else
        {
            cl_kernel_arg_address_qualifier addr_qual;
            clGetKernelArgInfo(info->kernel, arg, CL_KERNEL_ARG_ADDRESS_QUALIFIER, sizeof(addr_qual), &addr_qual, NULL);
            if (addr_qual == CL_KERNEL_ARG_ADDRESS_GLOBAL)
            {
                struct gpu_object *obj = *(void **)currentArg;
                print("AAA [%lld]\n", obj->mem);
                err = clSetKernelArg(
                    info->kernel,
                    arg++,
                    sizeof(cl_mem),
                    &obj->mem
                );
            }
            else
            {
                print("BBB\n");
                err = clSetKernelArg(
                    info->kernel,
                    arg++,
                    info->inputMap[i].size,
                    currentArg
                );
            }
            if (err)
            {
                print("Error: at clSetKernelArg %lld\n", err);
            }
            print("ok...\n");
        }
        currentArg += info->inputMap[i].size;
    }

    // queue kernel
    print("CONFIGURED: dims=%lld\n", dims);
    err = clEnqueueNDRangeKernel(
        SL_queues[SL_main_platform][0], 
        info->kernel, 
        dims,
        global_offset,
        global_size,
        NULL,
        0, NULL, NULL
    );
    if (err)
    {
        print("Error: at clEnqueueNDRangeKernel %lld\n", err);
    }
    
    ReleaseSRWLockExclusive(&info->kernel_lock);
    
    /* after return: push Errorcode to promise */
    int64_t value = err; // TODO: set error code instead of 0
    
    int64_t promise = *(int64_t *)&inputTable[inputTableSize - 8];
   
    // wait for answer

    x64PushObject(promise, &value, 0, 8, RETURN_STATE_GPU_END, NULL);

    // return
}
