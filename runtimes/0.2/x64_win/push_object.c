#define _WIN32_WINNT _WIN32_WINNT_VISTA

#include <winsock2.h>
#include <ws2tcpip.h>
#include "windows.h"

#include "runtime_lib.h"
#include "remote.h"
#include "runtime.h"



__attribute__((sysv_abi))
void PushObject(int64_t object, void *source, int64_t offset, int64_t size, void *returnAddress, void *rbpValue)
{
    log("PushObject to remote\n");
    assert(((BYTE *)object)[-9] != 0);
    
    if (((BYTE *)object)[-10] == OBJECT_PROMISE)
    {
        ((BYTE *)object)[-11] = 1;
    }
    switch (size)
    {
        case -1:
            ((BYTE *)object + offset)[0] = (BYTE)(int64_t)source;
            break;
        case -2:
            ((int16_t *)((BYTE *)object + offset))[0] = (int16_t)(int64_t)source;
            break;
        case -4:
            ((int32_t *)((BYTE *)object + offset))[0] = (int32_t)(int64_t)source;
            break;
        case -8:
            ((int64_t *)((BYTE *)object + offset))[0] = (int64_t)(int64_t)source;
            break;
        default:
            memcpy((BYTE *)object + offset, source, size);
    }

    (void)returnAddress;
    (void)rbpValue;
    return;
}

