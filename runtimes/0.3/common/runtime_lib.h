#ifndef RUNTIME_LIB
#define RUNTIME_LIB

#include "system.h"
#include "inttypes.h"


typedef uint8_t BYTE;


#ifdef FREESTANDING
    #ifndef _WIN32
        #error Unable to build nostdlib version not on windows.
    #endif
    [[nodiscard]] void *myMalloc(int64_t size);
    void myFree(void *mem);
    [[nodiscard]] void *myRealloc(void *mem, int64_t size);
#else
    #include "errno.h"
    #include "string.h"
    #include "stdlib.h"
    #include "stdio.h"
    #include "signal.h"
    #define myMalloc(x) calloc(1, (x))
    #define myFree(x) free(x)
    #define myRealloc(x, y) realloc(x, y)
#endif


int64_t myScanI64();
void myScanS(char *);
void *memcpy(void *dst, const void *src, size_t size);
int64_t GetTicks();
int64_t TicksToMicroseconds(int64_t ticks);
int64_t MicrosecondsToTicks(int64_t ticks);
int64_t SheduleTimeoutFromNow(int64_t microseconds);
void init_lib();


int64_t myAbs(int64_t);


[[noreturn]] void assertion_failure(const char* file, int64_t line, const char* func, const char* expr);


#ifdef _WIN32
    void myPrintf(const wchar_t *format_string, ...);
    int64_t myAtoll(wchar_t *number);
    #define print(f, ...) myPrintf(L ## f, __VA_ARGS__)
#else
    #ifdef FREESTANDING
        #error Cant build freestanding library under linux
    #endif
    #define myPrintf printf;
    #define print(f, ...) printf(f __VA_OPT__(,) __VA_ARGS__)
    int64_t myAtoll(const char *number);
#endif


#ifndef NDEBUG
    #define log(...) print(__VA_ARGS__)
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
