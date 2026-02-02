#define UNICODE 1
#define _UNICODE 1
#define _WIN32_WINNT _WIN32_WINNT_VISTA

#include "inttypes.h"
#include "x64.h"


extern int x64AsmExecuteWorker(void *, int64_t, void *, BYTE *);
void x64ExecuteWorker(struct queued_worker *worker)
{
    myPrintf(L"worker=%p\n", worker);
    
    x64AsmExecuteWorker(
        worker->data,
        worker->rdiValue,
        worker->rbpValue,
        (BYTE *)worker->context
    );
}
