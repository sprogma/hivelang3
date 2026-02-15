#define _WIN32_WINNT _WIN32_WINNT_VISTA

#include <winsock2.h>
#include <ws2tcpip.h>
#include "windows.h"

#include "../runtime_lib.h"
#include "../remote.h"
#include "../runtime.h"

#include "x64.h"


struct wait_timer
{
    int64_t endTime;
};

//@reg WK_STATE_TIMER_WAIT_X64 x64SleepStates
int64_t x64SleepStates(struct waiting_worker *w, int64_t ticks, int64_t *rdiValue)
{
    (void)rdiValue;
    
    switch (w->state)
    {

    case WK_STATE_QUERY_OBJECT_WAIT_X64:
        struct wait_timer *info = w->state_data;
        if (ticks >= info->endTime)
        {
            myFree(info);
            return 1;
        }
        return 0;
    }
    unreachable;
}

__attribute__((sysv_abi))
int64_t x64Sleep(int64_t time, int64_t _1, int64_t _2, int64_t _3, void *returnAddress, void *rbpValue)
{
    (void)_1;
    (void)_2;
    (void)_3;
    
    log("Sleep time %lld\n", time);
    struct wait_timer *query = myMalloc(sizeof(*query));
    *query = (struct wait_timer){
        .endTime = SheduleTimeoutFromNow(time*1000),
    };
    universalPauseWorker(returnAddress, rbpValue, WK_STATE_TIMER_WAIT_X64, query);
    struct thread_data* lc_data = TlsGetValue(dwTlsIndex);
    longjmpUN(&lc_data->ShedulerBuffer, 1);
}

