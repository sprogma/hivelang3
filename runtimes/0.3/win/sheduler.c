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

#include "gpu_subsystem.h"
#include "providers.h"

#include "x64/x64.h"
#include "gpu/gpu.h"
#include "dll/dll.h"
#include "loc/loc.h"



void SheduleWorker(struct thread_data *lc_data)
{
    setjmpUN(&lc_data->ShedulerBuffer);
    lc_data->stallable = 0;

    // call next worker

    struct queued_worker *curr = queue_extract(lc_data->number);
    if (curr)
    {
        lc_data->executedTasks++;
        log("\nSheduling new worker\n");
        log("Continue worker %lld from data=%p [rdi=%llx] [context=%p] [rbp=%p]\n",
                curr->id, curr->data, curr->rdiValue, curr->context, curr->rbpValue);
        lc_data->runningId = curr->id;
        lc_data->runningDepth = curr->depth;
        
        lc_data->stallable = Providers[Workers[curr->id].provider].stallable;
        lc_data->lastWorkerStart = GetTicks();
        Providers[Workers[curr->id].provider].ExecuteWorker(curr);
        lc_data->stallable = 0;
        // free current worker
        lc_data->completedTasks++;
        myFree(curr->rbpValue - 1024);
        myFree(curr);
    }
    UpdateWaitingWorkers();
}

struct master_sheduler_info
{
    HANDLE *hThreads;
    struct sheduler_instance_info **shedulers;
    volatile int64_t waitForExit;
};

struct sheduler_instance_info
{
    int64_t number;
    int64_t resCodeId;
    struct master_sheduler_info *masterInfo;
    struct thread_data *data;
};


HANDLE hContinueEvent; 

DWORD ShedulerInstance(void *vparam)
{
    struct sheduler_instance_info *param = vparam;
    struct thread_data *lc_data = myMalloc(sizeof(*lc_data));
    int64_t resCodeId = param->resCodeId;
    
    TlsSetValue(dwTlsIndex, lc_data);
    param->data = lc_data;
    
    SetEvent(hContinueEvent); 
    
    lc_data->number = (int64_t)param->number;
    lc_data->completedTasks = 0;
    while (1)
    {
        SheduleWorker(lc_data);

        // if resCodeId is ready - print it and return
        struct object_promise *p = (void *)GetHashtable(&local_objects, (BYTE *)&resCodeId, 8, 0);
        if (p != NULL)
        {
            p = (void *)((BYTE *)p - DATA_OFFSET(*p));
            if (p->ready)
            {
                print("ShedulerInstance completed\n");
                return 0;
            }
            // print("promise not set\n");
        }
    }
    myFree(lc_data);
    return 0;
}

void TryStallSheduler(struct master_sheduler_info *info, int64_t id, int64_t runnedTicks)
{
    struct sheduler_instance_info *shinfo = info->shedulers[id];
    struct thread_data *thdata = shinfo->data;
    
    SuspendThread(info->hThreads[id]);
    if (Providers[Workers[thdata->runningId].provider].TryStallWorker(info->hThreads[id], thdata, runnedTicks))
    {
        thdata->stalledTasks++;
    }
    ResumeThread(info->hThreads[id]);
}

DWORD MasterSheduler(void *vparam)
{
    struct master_sheduler_info *info = vparam;
    {
        print("|");
        for (int64_t i = 0; i < NUM_THREADS; ++i)
        {
            print("       thread %02x      |", i);
        }
        print("  Wait | Queue | RPmiss | ROreq | RIreq |\n");
        print("|");
        for (int64_t i = 0; i < NUM_THREADS; ++i)
        {
            print("  exec / done / stall |");
        }
        print("       |       |        |       |       |\n");
    }
    // watch for all threads
    int64_t prevPrint = GetTicks();
    int64_t chunk_time_ticks = MicrosecondsToTicks(CHUNK_TIME_US);
    while (!info->waitForExit)
    {
        // do we need to display progress counters?
        int64_t now = GetTicks();
        if (now - prevPrint > MicrosecondsToTicks(100000))
        {
            print("|");
            for (int64_t i = 0; i < NUM_THREADS; ++i)
            {
                int64_t exec = atomic_exchange(&info->shedulers[i]->data->executedTasks, 0);
                int64_t done = atomic_exchange(&info->shedulers[i]->data->completedTasks, 0);
                int64_t stall = atomic_exchange(&info->shedulers[i]->data->stalledTasks, 0);
                print(" %7lld %6lld %5lld |", exec, done, stall);
            }
            int64_t rpmiss = atomic_exchange(&glbStatRemotePathMisses, 0);
            int64_t roreq = atomic_exchange(&glbStatRemoteOutputRequests, 0);
            int64_t rireq = atomic_exchange(&glbStatRemoteInputRequests, 0);
            print(" %5lld | %5lld | %6lld | %5lld | %5lld |\n", wait_list_len, queue_size, rpmiss, roreq, rireq);
            prevPrint = now;
        }

        // check all threads - to swap them, if they are too long
        for (int64_t i = 0; i < NUM_THREADS; ++i)
        {
            if (info->shedulers[i]->data->stallable)
            {
                int64_t ticks = now - info->shedulers[i]->data->lastWorkerStart;
                int64_t time = TicksToMicroseconds(now - info->shedulers[i]->data->lastWorkerStart);
                log("runned time: %lld [from %lld]\n", time / 1000, TicksToMicroseconds(chunk_time_ticks) / 1000);
                if (ticks > chunk_time_ticks)
                {
                    /* try to stall thread */
                    TryStallSheduler(info, i, ticks);
                }
            }
        }
        UpdateWaitingWorkers();
        Sleep(10);
    }
    return 0;
}


// for debug
struct master_sheduler_info *glbMasterInfo;


int64_t ShedulerStart(int64_t resCodeId)
{
    struct master_sheduler_info *masterInfo = myMalloc(sizeof(*masterInfo));
    glbMasterInfo = masterInfo;
    masterInfo->hThreads = myMalloc(sizeof(*masterInfo->hThreads) * NUM_THREADS);
    masterInfo->shedulers = myMalloc(sizeof(*masterInfo->shedulers) * NUM_THREADS);

    hContinueEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    
    for (int64_t i = 0; i < NUM_THREADS; ++i)
    {
        struct sheduler_instance_info *info = myMalloc(sizeof(*info));
        info->number = i;
        info->resCodeId = resCodeId;
        info->masterInfo = masterInfo;
        masterInfo->shedulers[i] = info;
        DWORD threadId;
        masterInfo->hThreads[i] = CreateThread(NULL, 0, ShedulerInstance, info, 0, &threadId);
        if (masterInfo->hThreads[i] == NULL)
        {
            print("Failed to create thread %lld\n", i);
            return 1;
        }
        WaitForSingleObject(hContinueEvent, INFINITE);
    }

    CloseHandle(hContinueEvent);

    // start master thread
    DWORD masterId;
    HANDLE hMaster = CreateThread(NULL, 0, MasterSheduler, masterInfo, 0, &masterId);
    
    DWORD waitResult;
    waitResult = WaitForMultipleObjects(NUM_THREADS, masterInfo->hThreads, TRUE, INFINITE);
    masterInfo->waitForExit = 1;
    waitResult = WaitForSingleObject(hMaster, INFINITE);
    (void)waitResult;

    return 0;
}
