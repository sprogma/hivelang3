#ifndef RUNTIME_LIB
#define RUNTIME_LIB


#include "windows.h"
#include "inttypes.h"


typedef uint8_t BYTE;


void myPrintf(const wchar_t *format_string, ...);
int64_t myScanI64();
void myScanS(char *);
void *memcpy(void *dst, const void *src, size_t size);
int64_t myAtoll(wchar_t *number);
void *myMalloc(int64_t size);
void myFree(void *mem);
int64_t GetTicks();
int64_t TicksToMicroseconds(int64_t ticks);
int64_t MicrosecondsToTicks(int64_t ticks);
int64_t SheduleTimeoutFromNow(int64_t microseconds);
void init_lib();

int64_t myAbs(int64_t);

[[noreturn]] void assertion_failure(const char* file, int64_t line, const char* func, const char* expr);

// #define print(f, ...) myPrintf(L"%s:" L ## f, __FILE_NAME__, __VA_ARGS__)
#define print(f, ...) myPrintf(L ## f, __VA_ARGS__)


#ifndef NDEBUG
    #define log(f, ...) myPrintf(L ## f, __VA_ARGS__)
#else
    #define log(...)
#endif

#ifndef NDEBUG
    #define assert(expr) \
        if (!(expr)) { \
        assertion_failure(__FILE__, __LINE__, __func__, #expr); \
        }
#else
    #define assert(expr)
#endif


#endif
