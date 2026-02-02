#define _WIN32_WINNT _WIN32_WINNT_VISTA

#include <winsock2.h>
#include <ws2tcpip.h>
#include "windows.h"

#include "../runtime_lib.h"
#include "../remote.h"
#include "../runtime.h"

#include "gpu.h"

void gpuPauseWorker(void *returnAddress, void *rbpValue, struct waiting_cause *waiting_data)
{
    (void)returnAddress;
    (void)rbpValue;
    (void)waiting_data;
    print("ERROR: It is impossible to pause GPU worker [code is wrong]\n");
    ExitProcess(1);
}

int64_t gpuUpdateWaitingWorker(struct waiting_worker *w, int64_t ticks, int64_t *rdiValue)
{
    (void)w;
    (void)ticks;
    (void)rdiValue;
    print("ERROR: GPU workers can't wait\n");
    ExitProcess(1);
    return 0;
}
