#define _WIN32_WINNT _WIN32_WINNT_VISTA

#include <winsock2.h>
#include <ws2tcpip.h>
#include "windows.h"

#include "../runtime_lib.h"
#include "../remote.h"
#include "../runtime.h"

#include "dll.h"

__attribute__((sysv_abi))
void dllCallObject(int64_t moditifer, BYTE *args, int64_t workerId, int64_t _, void *returnAddress, void *rbpValue)
{
    (void)returnAddress;
    (void)rbpValue;
    
    int64_t tableSize = Workers[workerId].inputSize;

    log("Calling dllimport worker %lld [data=%p, mod=%lld]\n", workerId, args, moditifer);
    log("Table = ");
    for (int64_t i = 0; i < tableSize; ++i)
    {
        log("%02x ", args[i]);
    }
    log("\n");

    StartNewWorker(workerId, moditifer, args);
}
