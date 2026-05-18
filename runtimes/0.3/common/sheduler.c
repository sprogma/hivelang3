#include "system.h"

#include "inttypes.h"

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


#ifndef _WIN32



#endif


thread_result_t WaitListUpdateWorker(void *data)
{
    volatile int *waitForExit = data;
    while (!waitForExit)
    {
        UpdateWaitingWorkers();
        Sleep(100);
    }
    return 0;
}


struct master_sheduler_info
{
    thread_t *hThreads;
    struct sheduler_instance_info **shedulers;
    int64_t resCodeId;
    volatile int64_t waitForExit;
};

struct sheduler_instance_info
{
    int64_t number;
    struct master_sheduler_info *masterInfo;
    struct thread_data *data;
    volatile int64_t waitForExit;
};


void SheduleWorker(struct thread_data *lc_data, struct sheduler_instance_info *info)
{
    setjmpUN(&lc_data->ShedulerBuffer);
    lc_data->stallable = 0;
    
    if (info->waitForExit)
    {
        return;
    }

    // call next worker

    struct queued_worker *curr = scheduler_dequeue(&glb_scheduler, lc_data->number);
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
    }
    else
    {
        // if there is no tasks - try to find some new tasks
        UpdateWaitingWorkers();
    }
}


#ifdef _WIN32
HANDLE hContinueEvent; 
#else
sem_t hContinueEvent;
#endif

thread_result_t ShedulerInstance(void *vparam)
{
    struct sheduler_instance_info *param = vparam;
    struct thread_data *lc_data = myMalloc(sizeof(*lc_data));

    #ifdef _WIN32
    TlsSetValue(dwTlsIndex, lc_data);
    #else
    pthread_setspecific(dwTlsIndex, lc_data);
    #endif
    
    param->data = lc_data;

    #ifdef _WIN32
    SetEvent(hContinueEvent); 
    #else
    sem_post(&hContinueEvent);
    #endif
    
    lc_data->number = (int64_t)param->number;
    lc_data->completedTasks = 0;
    while (!param->waitForExit)
    {
        SheduleWorker(lc_data, param);
    }
    myFree(lc_data);
    return 0;
}

void TryStallSheduler(struct master_sheduler_info *info, int64_t id, int64_t runnedTicks)
{
    struct sheduler_instance_info *shinfo = info->shedulers[id];
    struct thread_data *thdata = shinfo->data;

    #ifdef _WIN32
    SuspendThread(info->hThreads[id]);
    if (Providers[Workers[thdata->runningId].provider].TryStallWorker(info->hThreads[id], thdata, runnedTicks))
    {
        thdata->stalledTasks++;
    }
    ResumeThread(info->hThreads[id]);
    #else
    if (Providers[Workers[thdata->runningId].provider].TryStallWorker(info->hThreads[id], thdata, runnedTicks))
    {
        thdata->stalledTasks++;
    }
    #endif
}

thread_result_t MasterSheduler(void *vparam)
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
    int64_t sent = 0;
    while (!info->waitForExit)
    {
        // do we need to display progress counters?
        int64_t now = GetTicks();
        if (printStats)
        {
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
                print(" %5lld | %5lld | %6lld | %5lld | %5lld |\n", wait_list_len, glb_scheduler.len, rpmiss, roreq, rireq);
                prevPrint = now;
            }
        }

        // check is there promise ready?        
        // if resCodeId is ready - print it and return
        if (!sent)
        {
            struct object_promise *p = (void *)i64GetHashtable(&local_objects, info->resCodeId);
            if (p != NULL)
            {
                p = (void *)((BYTE *)p - DATA_OFFSET(*p));
                if (p->ready)
                {
                    sent = 1;
                    print("Program calculation end.\n");
                    // send sign to all workers to stop
                    for (int64_t i = 0; i < NUM_THREADS; ++i)
                    {
                        info->shedulers[i]->waitForExit = 1;
                    }
                }
                // print("promise not set\n");
            }
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
    masterInfo->resCodeId = resCodeId;
    masterInfo->waitForExit = 0;

    #ifdef _WIN32
    hContinueEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    #else
    sem_init(&hContinueEvent, 0, 0);
    #endif
    
    for (int64_t i = 0; i < NUM_THREADS; ++i)
    {
        struct sheduler_instance_info *info = myMalloc(sizeof(*info));
        info->number = i;
        info->masterInfo = masterInfo;
        info->waitForExit = 0;
        masterInfo->shedulers[i] = info;

        #ifdef _WIN32
        DWORD threadId;
        masterInfo->hThreads[i] = CreateThread(NULL, 0, ShedulerInstance, info, 0, &threadId);
        if (masterInfo->hThreads[i] == NULL)
        {
            print("Failed to create thread %lld\n", i);
            return 1;
        }
        #else
        if (pthread_create(&masterInfo->hThreads[i], NULL, ShedulerInstance, info))
        {
            print("Failed to create thread %lld\n", i);
            return 1;
        }
        #endif

        
        #ifdef _WIN32
        WaitForSingleObject(hContinueEvent, INFINITE);
        ResetEvent(hContinueEvent);
        #else
        sem_wait(&hContinueEvent);
        #endif
    }

    #ifdef _WIN32
    CloseHandle(hContinueEvent);
    #else
    sem_destroy(&hContinueEvent);
    #endif

    // start WaitWorker thread
    _Atomic int64_t end;
    thread_t hUpdater;
    #ifdef _WIN32
    DWORD updaterId;
    hUpdater = CreateThread(NULL, 0, WaitListUpdateWorker, &end, 0, &updaterId);
    #else
    pthread_create(&hUpdater, NULL, WaitListUpdateWorker, &end);
    #endif

    // start master thread
    thread_t hMaster;
    #ifdef _WIN32
    DWORD masterId;
    hMaster = CreateThread(NULL, 0, MasterSheduler, masterInfo, 0, &masterId);
    #else
    pthread_create(&hMaster, NULL, MasterSheduler, masterInfo);
    #endif

    #ifdef _WIN32
    DWORD waitResult;
    waitResult = WaitForMultipleObjects(NUM_THREADS, masterInfo->hThreads, TRUE, INFINITE);
    masterInfo->waitForExit = 1;
    end = 1;
    waitResult = WaitForSingleObject(hMaster, INFINITE);
    waitResult = WaitForSingleObject(hUpdater, INFINITE);
    (void)waitResult;
    #else
    for (int i = 0; i < NUM_THREADS; i++) 
    {
        pthread_join(masterInfo->hThreads[i], NULL);
    }
    masterInfo->waitForExit = 1;
    end = 1;
    pthread_join(hMaster, NULL);
    pthread_join(hUpdater, NULL);
    #endif

    return 0;
}

