#define _WIN32_WINNT _WIN32_WINNT_VISTA

#include <winsock2.h>
#include <ws2tcpip.h>
#include "windows.h"

#include "../runtime_lib.h"
#include "../remote.h"
#include "../runtime.h"

#include "x64.h"

__attribute__((sysv_abi))
void x64CallObject(int64_t moditifer, BYTE *args, int64_t workerId, int64_t _, void *returnAddress, void *rbpValue)
{
    (void)returnAddress;
    (void)rbpValue;
    
    int64_t tableSize = Workers[workerId].inputSize;

    log("Calling worker %lld [data=%p, mod=%lld]\n", workerId, args, moditifer);
    log("Table = ");
    for (int64_t i = 0; i < tableSize; ++i)
    {
        log("%02x ", args[i]);
    }
    log("\n");

    StartNewWorker(workerId, moditifer, args);
}
