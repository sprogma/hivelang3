#define _WIN32_WINNT _WIN32_WINNT_VISTA

#include <winsock2.h>
#include <ws2tcpip.h>
#include "windows.h"

#include "runtime_lib.h"
#include "remote.h"
#include "runtime.h"



void UpdateLocalPush(void *obj, int64_t offset, int64_t size, void *source)
{
    if (((BYTE *)obj)[-1] == OBJECT_PROMISE)
    {
        ((BYTE *)obj)[-2] = 1;
    }
    switch (size)
    {
        case -1:
            ((BYTE *)obj + offset)[0] = (BYTE)(int64_t)source;
            break;
        case -2:
            ((int16_t *)((BYTE *)obj + offset))[0] = (int16_t)(int64_t)source;
            break;
        case -4:
            ((int32_t *)((BYTE *)obj + offset))[0] = (int32_t)(int64_t)source;
            break;
        case -8:
            ((int64_t *)((BYTE *)obj + offset))[0] = (int64_t)(int64_t)source;
            break;
        default:
            memcpy((BYTE *)obj + offset, source, size);
    }
}


__attribute__((sysv_abi))
void PushObject(int64_t object_id, void *source, int64_t offset, int64_t size, void *returnAddress, void *rbpValue)
{
    BYTE *obj = (BYTE *)GetHashtable(&local_objects, (BYTE *)&object_id, 8, 0);
    if (obj == 0)
    {
        // remote object
        (void)returnAddress;
        (void)rbpValue;
        print("NOT IMPLEMENTED: push to remote object\n");
    }
    else
    {
        UpdateLocalPush(obj, offset, size, source);
    }
}

