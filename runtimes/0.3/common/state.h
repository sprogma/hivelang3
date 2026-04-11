#ifndef STATE_H
#define STATE_H




//<<--Quote-->> from::ls *.c -r -Ex state.h|sls "^\s*//@reg\w*\s+(\w+)\s+(\w+)$"|%{"// $(rvpa -relative $_.Path):$($_.LineNumber):$($_.Line)"}
// ./x64/new_object.c:104://@reg WK_STATE_NEW_OBJECT_WAIT_PAGES_X64 x64NewObjectStates
// ./x64/push_object.c:47://@regPush WK_STATE_PUSH_OBJECT_WAIT_X64 x64OnPushObject
// ./x64/push_object.c:65://@reg WK_STATE_PUSH_OBJECT_WAIT_X64 x64PushObjectStates
// ./x64/query_object.c:42://@regQuery WK_STATE_QUERY_OBJECT_WAIT_X64 x64OnQueryObject
// ./x64/query_object.c:66://@reg WK_STATE_QUERY_OBJECT_WAIT_X64 x64QueryObjectStates
// ./x64/sleep.c:17://@reg WK_STATE_TIMER_WAIT_X64 x64SleepStates
// ./cast_provider.c:66://@regQuery WK_STATE_GET_OBJECT_SIZE castOnQueryObject
// ./cast_provider.c:67://@regQuery WK_STATE_GET_OBJECT_DATA castOnQueryObject
// ./cast_provider.c:93://@reg WK_STATE_GET_OBJECT_SIZE anyCastStates
// ./cast_provider.c:94://@reg WK_STATE_GET_OBJECT_SIZE_RESULT anyCastStates
// ./cast_provider.c:95://@reg WK_STATE_GET_OBJECT_DATA anyCastStates
// ./cast_provider.c:96://@reg WK_STATE_GET_OBJECT_DATA_RESULT anyCastStates
// ./cast_provider.c:97://@reg WK_STATE_CAST_WAIT_PAGES anyCastStates
//<<--QuoteEnd-->>

enum worker_wait_state
{
    //<<--Quote-->> from::(ls *.c -r -Ex state.h|sls "^\s*//@reg\w*\s+(\w+)\s+(\w+)$"|% Matches|%{"$(" "*4)$($_.Groups[1])"}|s -u)-join",`n"
    WK_STATE_NEW_OBJECT_WAIT_PAGES_X64,
    WK_STATE_PUSH_OBJECT_WAIT_X64,
    WK_STATE_QUERY_OBJECT_WAIT_X64,
    WK_STATE_TIMER_WAIT_X64,
    WK_STATE_GET_OBJECT_SIZE,
    WK_STATE_GET_OBJECT_DATA,
    WK_STATE_GET_OBJECT_SIZE_RESULT,
    WK_STATE_GET_OBJECT_DATA_RESULT,
    WK_STATE_CAST_WAIT_PAGES
    //<<--QuoteEnd-->>
};



#endif
