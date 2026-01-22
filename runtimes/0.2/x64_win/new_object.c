#define _WIN32_WINNT _WIN32_WINNT_VISTA

#include <winsock2.h>
#include <ws2tcpip.h>
#include "windows.h"

#include "runtime_lib.h"
#include "remote.h"
#include "runtime.h"


// if allocating ARRAY, param must be element size.
// [it can be used to split big arrays on diffrent hives]
// if allocating OBJECT, param = 1
// if allocating PROMISE/PIPE, param is unused
__attribute__((sysv_abi))
int64_t NewObject(int64_t type, int64_t size, int64_t param, void *returnAddress, void *rbpValue)
{
    // find non empty memory page
    int64_t remote_id = 0, set = 0;
    AcquireSRWLockShared(&pages_lock);
    for (int64_t i = 0; i < pages_len; ++i)
    {
        if (pages[i].next_allocated_id < OBJECTS_PER_PAGE)
        {
            remote_id = pages[i].next_allocated_id++;
            set = 1;
            break;
        }
    }
    ReleaseSRWLockShared(&pages_lock);

    if (set == 0)
    {
        if (returnAddress == NULL)
        {
            return 0;
        }
        /* wait for new pages */
        struct waiting_pages cause;
        PauseWorker(returnAddress, rbpValue, (struct waiting_cause *)&cause);
        longjmpUN(&ShedulerBuffer, 1);
    }
    
    // generate header
    struct object header;
    header.type = type;
    header.is_remote = 0;
    header.remote_id = remote_id;

    // create object
    switch (type)
    {
        case OBJECT_ARRAY:
        {
            log("Array of %lld bytes, element of size %lld allocated\n", size, param);
            struct object_array *res = myMalloc(sizeof(*res) + size);
            memcpy((BYTE *)res + DATA_OFFSET(*res) - DATA_OFFSET(header), &header, sizeof(header));
            res->length = size / param;
            int64_t id = (int64_t)res + DATA_OFFSET(*res);
            object_array[object_array_len++] = (struct object *)id;
            log("[id=%llx]\n", id);
            return id;
        }
        case OBJECT_PROMISE:
        {
            log("Promise for size %lld allocated\n", size);
            struct object_promise *res = myMalloc(sizeof(*res) + size);
            memcpy((BYTE *)res + DATA_OFFSET(*res) - DATA_OFFSET(header), &header, sizeof(header));
            res->type = OBJECT_PROMISE;
            res->ready = 0;
            int64_t id = (int64_t)res + DATA_OFFSET(*res);
            object_array[object_array_len++] = (struct object *)id;
            log("[id=%llx]\n", id);
            return id;
        }
        case OBJECT_OBJECT:
        {
            log("Class for size %lld allocated\n", size);
            struct object_object *res = myMalloc(sizeof(*res) + size);
            res->type = OBJECT_OBJECT;
            int64_t id = (int64_t)res + DATA_OFFSET(*res);
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
            memcpy(res->_, &param, 8);
            res->type = OBJECT_ARRAY;
            res->length = objSize / param;
            // fill data
            memcpy(res->data, objStart, objSize);
            for (int i = 0; i < objSize; ++i)
            {
                log(" %02x", res->data[i]);
            }
            log("\n");
            int64_t id = (int64_t)res + DATA_OFFSET(*res);
            object_array[object_array_len++] = (struct object *)id;
            log("[id=%llx]\n", id);
            return id;
        }
        default:
            log("Wrong type in NewObject\n");
    }
    return -1;
}
