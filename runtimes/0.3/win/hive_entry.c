#include "runtime.h"
#include "runtime_api.h"
#include "remote.h"
#include "x64/x64.h"


int64_t StartInitialProcess(int64_t entryWorker, int64_t *cmdArgs, int64_t cmdArgsLen)
{
    int64_t inputId = 0, resCodeId = 0;
    
    print("waiting startup pages\n");

    while (inputId == 0)
    {
        inputId = x64NewObject(3, 8 * cmdArgsLen, 8, 0, NULL, NULL);
        Sleep(10);
    }

    struct object *obj = (void *)GetHashtable(&local_objects, (BYTE *)&inputId, 8, 0);
    if (obj == 0)
    {
        print("Error: allocated array isn't local\n");
    }
    memcpy(obj, cmdArgs, 8 * cmdArgsLen);


    while (resCodeId == 0)
    {
        resCodeId = x64NewObject(2, 4, 4, 0, NULL, NULL);
        Sleep(10);
    }

    BYTE *tbl = myMalloc(16);
    memcpy(tbl + 0, &inputId, 8);
    memcpy(tbl + 8, &resCodeId, 8);

    log("resCodeId=%p\n", resCodeId);
    StartNewWorker(entryWorker, 1, tbl);

    myFree(tbl);

    return resCodeId;
}
