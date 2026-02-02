#define _WIN32_WINNT _WIN32_WINNT_VISTA

#include <winsock2.h>
#include <ws2tcpip.h>
#include "windows.h"

#include "../runtime_lib.h"
#include "../remote.h"
#include "../runtime.h"
#include "gpu.h"


__attribute__((sysv_abi))
void gpuPushObject(int64_t object_id, void *source, int64_t offset, int64_t size, void *returnAddress, void *rbpValue)
{
    (void)object_id;
    (void)source;
    (void)offset;
    (void)size;
    (void)returnAddress;
    (void)rbpValue;
    print("NOT SUPPORTED: push in gpu object\n");
}


__attribute__((sysv_abi))
void gpuPushPipe(int64_t object_id, void *source, int64_t offset, int64_t size, void *returnAddress, void *rbpValue)
{
    (void)object_id;
    (void)source;
    (void)offset;
    (void)size;
    (void)returnAddress;
    (void)rbpValue;
    print("NOT SUPPORTED: push in gpu object [pipe]\n");
}

