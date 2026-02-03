#define UNICODE 1
#define _UNICODE 1
#define _WIN32_WINNT _WIN32_WINNT_VISTA

#include "inttypes.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include "windows.h"

#include "runtime_lib.h"
#include "runtime_api.h"
#include "remote.h"
#include "runtime.h"

#include "providers.h"

#include "x64/x64.h"
#include "gpu/gpu.h"


__attribute__((sysv_abi))
int64_t anyCastProvider(void *obj, int64_t to, int64_t from, int64_t _, void *returnAddress, void *rbpValue)
{
    (void)returnAddress;
    (void)rbpValue;
    print("convert %p [types %lld -> %lld]\n", obj, from, to);
    if (from == PROVIDER_X64 && to == PROVIDER_GPU)
    {
        // get page        
        // create new object
        gpuNewObject(int64_t type, int64_t size, int64_t param, int64_t _, void *returnAddress, void *rbpValue)
        // fill it with data
        return (int64_t)obj;
    }
    else if (from == PROVIDER_GPU && to == PROVIDER_X64)
    {
        // get page
        // create new object
        // fill it with data
        return (int64_t)obj;
    }
    print("ERROR: not supported conversion between providers %lld to %lld\n", from, to);
    return 0;
}
