#define _WIN32_WINNT _WIN32_WINNT_VISTA

#include <winsock2.h>
#include <ws2tcpip.h>
#include "windows.h"

#include "../runtime_lib.h"
#include "../remote.h"
#include "../runtime.h"
#include "../providers.h"
#include "gpu.h"

void gpuNewObjectUsingPage(int64_t type, int64_t size, int64_t param, int64_t *remote_id)
{
    log("Allocating buffer of size %lld param %lld\n", size, param);
    struct gpu_object *res = myMalloc(sizeof(*res));
    res->size = size;
    res->length = size / param;
    int err;
    res->mem = gpuAlloc(size, CL_MEM_READ_WRITE, &err);
    if (err)
    {
        print("ERROR: gpuAlloc failed [%lld]\n", err);
    }
    *remote_id = (int64_t)res;
    print("result %p\n", res);
    
    // create object
    switch (type)
    {
        case OBJECT_PIPE:
        {
            print("ERROR: Pipes are unsupported on GPU provider\n");    
            return;
        }
        case OBJECT_ARRAY:
        case OBJECT_PROMISE:
        case OBJECT_OBJECT:
        {
            log("object for size %lld allocated\n", size);
            return;
        }
        case OBJECT_DEFINED_ARRAY:
        {
            print("ERROR: Pipes are unsupported on GPU provider for now\n");
            return;
        }
        default:
            log("Wrong type in NewObject\n");
    }
    return;
}


// if allocating ARRAY, param must be element size.
// [it can be used to split big arrays on diffrent hives]
// if allocating OBJECT, param = 1
// if allocating PROMISE/PIPE, param is unused
__attribute__((sysv_abi))
int64_t gpuNewObject(int64_t type, int64_t size, int64_t param, int64_t _, void *returnAddress, void *rbpValue)
{
    (void)returnAddress;
    (void)rbpValue;
    (void)_;
    
    // find non empty memory page
    int64_t remote_id;
    gpuNewObjectUsingPage(type, size, param, &remote_id);
    return remote_id;
}


