#define UNICODE 1
#define _UNICODE 1
#define _WIN32_WINNT _WIN32_WINNT_VISTA

#include "inttypes.h"
#include "x64.h"


extern int x64AsmExecuteWorker(void *, int64_t, void *, BYTE *);
void x64ExecuteWorker(struct queued_worker *worker)
{
    log("worker=%p\n", worker);
    // print("worker id=%lld\n", worker->id);
    
    x64AsmExecuteWorker(
        worker->data,
        worker->rdiValue,
        worker->rbpValue,
        (BYTE *)worker->context
    );
}

int64_t x64TryStallWorker(HANDLE hThread, struct thread_data *data, int64_t runnedTicks)
{
    log("Stalling worker at thread %p\n", hThread);
    
    CONTEXT ctx;
    ctx.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
    if (GetThreadContext(hThread, &ctx)) 
    {
        struct x64_worker_data *wkinfo = Workers[data->runningId].data;
        if (ctx.Rip >= (DWORD64)wkinfo->start && ctx.Rip <= (DWORD64)wkinfo->end)
        {
            /* save context and select next worker */
            struct queued_worker *t = myMalloc(sizeof(*t));
            t->context[0] = ctx.R8;  
            t->context[1] = ctx.R9;
            t->context[2] = ctx.R10; 
            t->context[3] = ctx.R11;
            t->context[4] = ctx.R12; 
            t->context[5] = ctx.Rbx;
            t->context[6] = ctx.R13; 
            t->context[7] = ctx.R14;
            t->context[8] = ctx.R15; 
            t->context[10] = ctx.Rsi; 
            t->context[11] = ctx.Rax;
            t->context[12] = ctx.Rcx; 
            t->context[13] = ctx.Rdx;
            t->id = data->runningId;
            t->depth = data->runningDepth / 2 - TicksToMicroseconds(runnedTicks) / 1000; // dercease priority
            t->data = (void *)ctx.Rip;
            t->rbpValue = (void *)ctx.Rbp;
            t->rdiValue = ctx.Rdi;

            log("Paused worker %lld [stall]: next address: %p, depth=%lld\n", data->runningId, ctx.Rip, t->depth);

            queue_enqueue(t);
            
            ctx.Rip = (DWORD64)longjmpUN;
            ctx.Rcx = (DWORD64)&data->ShedulerBuffer;
            ctx.Rdx = (DWORD64)1;

            SetThreadContext(hThread, &ctx);
            return 1;
        }
    }
    return 0;
}
