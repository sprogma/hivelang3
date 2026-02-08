#define _WIN32_WINNT _WIN32_WINNT_VISTA

#include <winsock2.h>
#include <ws2tcpip.h>
#include "windows.h"

#include "../runtime_lib.h"
#include "../remote.h"
#include "../runtime.h"
#include "../providers.h"
#include "x64.h"

void x64NewObjectUsingPage(int64_t type, int64_t size, int64_t param, int64_t *remote_id)
{
    // generate header
    struct object header;
    header.type = type;
    header.provider = PROVIDER_X64;
    header.data_size = size;

    // create object
    switch (type)
    {
        case OBJECT_PIPE:
        {
            log("Pipe for size %lld allocated\n", size);
            struct object_promise *res = myMalloc(sizeof(*res) + size);
            memcpy((BYTE *)res + DATA_OFFSET(*res) - DATA_OFFSET(header), &header, sizeof(header));
            res->ready = 0;
            int64_t pointer = (int64_t)res + DATA_OFFSET(*res);
            RegisterObjectWithId(*remote_id, (struct object *)pointer);
            log("[id=%llx]\n", *remote_id);
            return;
        }
        case OBJECT_ARRAY:
        {
            log("Array of %lld bytes, element of size %lld allocated\n", size, param);
            struct object_array *res = myMalloc(sizeof(*res) + size);
            memcpy((BYTE *)res + DATA_OFFSET(*res) - DATA_OFFSET(header), &header, sizeof(header));
            res->length = size / param;
            int64_t pointer = (int64_t)res + DATA_OFFSET(*res);
            RegisterObjectWithId(*remote_id, (struct object *)pointer);
            log("[id=%llx]\n", *remote_id);
            return;
        }
        case OBJECT_PROMISE:
        {
            log("Promise for size %lld allocated\n", size);
            struct object_promise *res = myMalloc(sizeof(*res) + size);
            memcpy((BYTE *)res + DATA_OFFSET(*res) - DATA_OFFSET(header), &header, sizeof(header));
            res->ready = 0;
            int64_t pointer = (int64_t)res + DATA_OFFSET(*res);
            RegisterObjectWithId(*remote_id, (struct object *)pointer);
            log("[id=%llx]\n", *remote_id);
            return;
        }
        case OBJECT_OBJECT:
        {
            log("Class for size %lld allocated\n", size);
            struct object_object *res = myMalloc(sizeof(*res) + size);
            memcpy((BYTE *)res + DATA_OFFSET(*res) - DATA_OFFSET(header), &header, sizeof(header));
            int64_t pointer = (int64_t)res + DATA_OFFSET(*res);
            RegisterObjectWithId(*remote_id, (struct object *)pointer);
            log("[id=%llx]\n", *remote_id);
            return;
        }
        case OBJECT_DEFINED_ARRAY:
        {
            // size is ID
            BYTE *objStart = defined_arrays[size].start;
            int64_t objSize = defined_arrays[size].size;
            log("Defined array for size %lld allocated\n", objSize);
            struct object_array *res = myMalloc(sizeof(*res) + objSize);
            memcpy((BYTE *)res + DATA_OFFSET(*res) - DATA_OFFSET(header), &header, sizeof(header));
            res->length = objSize / param;
            res->type = OBJECT_ARRAY;
            // fill data
            memcpy(res->data, objStart, objSize);
            for (int i = 0; i < objSize; ++i)
            {
                log(" %02x", res->data[i]);
            }
            log("\n");
            int64_t pointer = (int64_t)res + DATA_OFFSET(*res);
            RegisterObjectWithId(*remote_id, (struct object *)pointer);
            log("[id=%llx]\n", *remote_id);
            return;
        }
        default:
            log("Wrong type in NewObject\n");
    }
    return;
}


struct wait_pages_info
{
    int64_t obj_type;
    int64_t size;
    int64_t param;
};
//@reg WK_STATE_NEW_OBJECT_WAIT_PAGES_X64 x64NewObjectStates
int64_t x64NewObjectStates(struct waiting_worker *w, int64_t ticks, int64_t *rdiValue)
{
    (void)ticks;
    
    switch (w->state)
    {
    
    case WK_STATE_NEW_OBJECT_WAIT_PAGES_X64:
        struct wait_pages_info *info = (struct wait_pages_info *)w->state_data;
        int64_t new_id;
        if (GetNewObjectId(&new_id))
        {
            x64NewObjectUsingPage(info->obj_type, info->size, info->param, &new_id);
            *rdiValue = new_id;
            myFree(info);
            return 1;
        }
        return 0;
    }
    
    __builtin_unreachable();
}

// if allocating ARRAY, param must be element size.
// [it can be used to split big arrays on diffrent hives]
// if allocating OBJECT, param = 1
// if allocating PROMISE/PIPE, param is unused
__attribute__((sysv_abi))
int64_t x64NewObject(int64_t type, int64_t size, int64_t param, int64_t _, void *returnAddress, void *rbpValue)
{
    (void)_;
    // find non empty memory page
    int64_t remote_id;
    
    if (!GetNewObjectId(&remote_id))
    {
        if (returnAddress == NULL)
        {
            return 0;
        }
        /* wait for new pages */
        struct wait_pages_info *info = myMalloc(sizeof(*info));
        info->obj_type = type,
        info->size = size,
        info->param = param,
        universalPauseWorker(returnAddress, rbpValue, WK_STATE_NEW_OBJECT_WAIT_PAGES_X64, info);

        struct thread_data* lc_data = TlsGetValue(dwTlsIndex);
        longjmpUN(&lc_data->ShedulerBuffer, 1);
    }

    x64NewObjectUsingPage(type, size, param, &remote_id);
    return remote_id;
}


