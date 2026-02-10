#define _WIN32_WINNT _WIN32_WINNT_VISTA

#include <winsock2.h>
#include <ws2tcpip.h>
#include "windows.h"

#include "../runtime_lib.h"
#include "../remote.h"
#include "../runtime.h"
#include "../providers.h"
#include "loc.h"

void locNewObjectUsingPage(int64_t type, int64_t size, int64_t param, int64_t *remote_id)
{
    // generate header
    struct object header;
    header.type = type;
    header.provider = PROVIDER_LOC;
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
            *remote_id = pointer;
            return;
        }
        case OBJECT_ARRAY:
        {
            log("Array of %lld bytes, element of size %lld allocated\n", size, param);
            struct object_array *res = myMalloc(sizeof(*res) + size);
            memcpy((BYTE *)res + DATA_OFFSET(*res) - DATA_OFFSET(header), &header, sizeof(header));
            res->length = size / param;
            int64_t pointer = (int64_t)res + DATA_OFFSET(*res);
            *remote_id = pointer;
            return;
        }
        case OBJECT_PROMISE:
        {
            log("Promise for size %lld allocated\n", size);
            struct object_promise *res = myMalloc(sizeof(*res) + size);
            memcpy((BYTE *)res + DATA_OFFSET(*res) - DATA_OFFSET(header), &header, sizeof(header));
            res->ready = 0;
            int64_t pointer = (int64_t)res + DATA_OFFSET(*res);
            *remote_id = pointer;
            return;
        }
        case OBJECT_OBJECT:
        {
            log("Class for size %lld allocated\n", size);
            struct object_object *res = myMalloc(sizeof(*res) + size);
            memcpy((BYTE *)res + DATA_OFFSET(*res) - DATA_OFFSET(header), &header, sizeof(header));
            int64_t pointer = (int64_t)res + DATA_OFFSET(*res);
            *remote_id = pointer;
            return;
        }
        case OBJECT_DEFINED_ARRAY:
        {
            // size is ID
            BYTE *objStart = defined_arrays[size].start;
            int64_t objSize = defined_arrays[size].size;
            log("Defined array for size %lld allocated\n", objSize);
            struct object_array *res = myMalloc(sizeof(*res) + objSize);
            header.type = OBJECT_ARRAY;
            header.data_size = objSize;
            memcpy((BYTE *)res + DATA_OFFSET(*res) - DATA_OFFSET(header), &header, sizeof(header));
            res->length = objSize / param;
            // fill data
            memcpy(res->data, objStart, objSize);
            for (int i = 0; i < objSize; ++i)
            {
                log(" %02x", res->data[i]);
            }
            log("\n");
            int64_t pointer = (int64_t)res + DATA_OFFSET(*res);
            *remote_id = pointer;
            int64_t size = 0;
            memcpy(&size, (void *)pointer - 8, 6);
            log("[id=%llx] [size=%lld]\n", *remote_id, size);
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
int64_t locNewObject(int64_t type, int64_t size, int64_t param, int64_t _, void *returnAddress, void *rbpValue)
{
    (void)_;
    (void)returnAddress;
    (void)rbpValue;
    int64_t remote_id;
    locNewObjectUsingPage(type, size, param, &remote_id);
    return remote_id;
}


