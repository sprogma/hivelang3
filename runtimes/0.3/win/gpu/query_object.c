#define _WIN32_WINNT _WIN32_WINNT_VISTA

#include <winsock2.h>
#include <ws2tcpip.h>
#include "windows.h"

#include "../runtime_lib.h"
#include "../remote.h"
#include "../runtime.h"
#include "gpu.h"



__attribute__((sysv_abi))
int64_t gpuQueryObject(void *destination, int64_t object_id, int64_t offset, int64_t size, void *returnAddress, void *rbpValue)
{
    (void)object_id;
    (void)destination;
    (void)offset;
    (void)size;
    (void)returnAddress;
    (void)rbpValue;
    print("NOT SUPPORTED: query in gpu object\n");
    return 0;
}


__attribute__((sysv_abi))
int64_t gpuQueryPipe(void *destination, int64_t object_id, int64_t offset, int64_t size, void *returnAddress, void *rbpValue)
{
    (void)object_id;
    (void)destination;
    (void)offset;
    (void)size;
    (void)returnAddress;
    (void)rbpValue;
    print("NOT SUPPORTED: query in gpu object [pipe]\n");
    return 0;
}
