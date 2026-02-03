#ifndef STATE_H
#define STATE_H




//<<--Quote-->> from::ls *.c -r -Ex state.h|sls "^\s*//@reg\s+(\w+)\s+(\w+)$"|%{"// $(rvpa -relative $_.Path):$($_.LineNumber):$($_.Line)"}
// .\x64\new_object.c:96://@reg WK_STATE_PUSH_OBJECT_WAIT_X64 x64NewObjectMachine
// .\x64\push_object.c:49://@reg WK_STATE_PUSH_OBJECT_WAIT_X64 x64PushObjectWorker
//<<--QuoteEnd-->>

enum worker_wait_state
{
    WK_STATE_NEW_OBJECT_WAIT_PAGES_X64,
    WK_STATE_PUSH_OBJECT_WAIT_X64,
    WK_STATE_QUERY_OBJECT_WAIT_X64,
};



#endif
