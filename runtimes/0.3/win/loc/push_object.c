#define _WIN32_WINNT _WIN32_WINNT_VISTA

#include <winsock2.h>
#include <ws2tcpip.h>
#include "windows.h"

#include "../runtime_lib.h"
#include "../remote.h"
#include "../runtime.h"
#include "loc.h"


void locUpdateLocalPush(void *obj, int64_t offset, int64_t size, void *source)
{
    if (((BYTE *)obj)[-1] == OBJECT_PROMISE)
    {
        struct object_promise *objp = (void *)((int64_t)obj - DATA_OFFSET(*objp));
        objp->ready = 1;
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
