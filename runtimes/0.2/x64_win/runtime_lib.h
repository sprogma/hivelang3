#ifndef RUNTIME_LIB
#define RUNTIME_LIB


#include "inttypes.h"



void myPrintf(const wchar_t *format_string, ...);
int64_t myScanI64();
void myMemcpy(void *dst, void *src, int64_t size);
int64_t myAtoll(wchar_t *number);
void *myMalloc(int64_t size);
void myFree(void *mem);
void init_lib();


#define print(f, ...) myPrintf(L ## f, __VA_ARGS__)


#ifndef NDEBUG
    #define log(f, ...) myPrintf(L ## f, __VA_ARGS__)
#else
    #define log(...)
#endif


#endif
