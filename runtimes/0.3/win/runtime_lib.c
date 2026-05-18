#define UNICODE 1
#define _UNICODE 1

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
            
#include "stdarg.h"
#include "immintrin.h"
#include "inttypes.h"

#include "../common/runtime_lib.h"

// for generated
#ifdef FREESTANDING
void *memset(void *_dst, int value, size_t size)
{
    BYTE *dst = _dst;
    while (size--)
    {
        *dst++ = value;
    }
    return _dst;
}


void *memcpy(void *_dst, const void *_src, size_t size)
{
    if (size == 4) { *(int32_t *)_dst = *(int32_t *)_src; return _dst;}
    if (size == 8) { *(int64_t *)_dst = *(int64_t *)_src; return _dst;}
    BYTE *dst = _dst, *src = (void *)_src;
    #ifdef __AVX2__
        while (size >= 32)
        {
            __m256i ymm0 = _mm256_loadu_si256((void *)src);
            _mm256_storeu_si256((void *)dst, ymm0);
            src += 32;
            dst += 32;
            size -= 32;
        }
    #endif
    while (size >= 8)
    {
        *(int64_t *)dst = *(int64_t *)src;
        src += 8;
        dst += 8;
        size -= 8;
    }
    while (size--)
    {
        *dst++ = *src++;
    }
    return _dst;
}

int memcmp(const void *str1, const void *str2, size_t count) 
{
    const unsigned char *s1 = (const unsigned char *)str1;
    const unsigned char *s2 = (const unsigned char *)str2;
    while (count >= 8) 
    {
        if (*(const uint64_t *)s1 != *(const uint64_t *)s2) 
        {
            break;
        }
        s1 += 8;
        s2 += 8;
        count -= 8;
    }
    while (count > 0) 
    {
        if (*s1 != *s2) 
        {
            return (*s1 < *s2) ? -1 : 1;
        }
        s1++;
        s2++;
        count--;
    }

    return 0;
}

#endif


HANDLE hOutput;
HANDLE hInput;
HANDLE hHeap;
int64_t frequency;
    
#ifdef FREESTANDING
[[nodiscard]] void *myMalloc(int64_t size)
{
    void *mem = HeapAlloc(hHeap, HEAP_ZERO_MEMORY, size);
    if (mem == NULL)
    {
        print("Error: failed to allocate %lld bytes of memory: %lld\n", size, (int64_t)GetLastError());
    }
    return mem;
}

[[nodiscard]] void *myRealloc(void *mem, int64_t size)
{
    mem = HeapReAlloc(hHeap, HEAP_ZERO_MEMORY, mem, size);
    if (mem == NULL)
    {
        print("Error: failed to reallocate %lld bytes of memory: %lld\n", size, (int64_t)GetLastError());
    }
    return mem;
}

void myFree(void *mem)
{
    if (HeapFree(hHeap, 0, mem) == 0)
    {
        print("Error: failed to free memory: %lld\n", (int64_t)GetLastError());
    }
}
#endif

int64_t myAtoll(wchar_t *number)
{
    while (*number == ' ' || *number == '\n' || *number == '\t' || *number == '\r') number++;
    int res = 0, neg = 0;
    if (*number == '+')
    {
        number++;
    }
    else if (*number == '-')
    {
        number++;
        neg = 1;
    }
    while ('0' <= *number && *number <= '9')
    {
        res = ((*number) - '0') + res * 10;
        ++number;
    }
    return (neg ? -res : res);
}


char get_char()
{
    DWORD mode = 0;
    if (GetConsoleMode(hInput, &mode))
    {
        wchar_t ch = 0;
        DWORD read = 0;
        if (ReadConsoleW(hInput, &ch, 1, &read, NULL) && read == 1)
        {
            return ch;  
        }
    }
    else
    {
        wchar_t ch = 0;
        DWORD bytesRead = 0;
        if (ReadFile(hInput, &ch, 1, &bytesRead, NULL) && bytesRead == 1)
        {
            return ch;
        }
    }
    return L'\0';
}


int64_t myScanI64()
{
    int64_t res = 0, y = 0, t = 1;
    while (t)
    {
        wchar_t buf = get_char();
        if (buf < '0' || buf > '9')
        {
            t -= y;
        }
        else
        {
            res *= 10;
            res += buf - '0';
            y = 1;
        }
    }
    return res;
}

void myScanS(char *str)
{
    char *end = str;
    do
    {
        wchar_t buf = get_char();
        if (buf == 0)
        {
            end++;
            break;
        }
        *end++ = buf;
        if (end[-1] == ' ' && end == str + 1)
        {
            end--;
            continue;
        }
    }
    while (end[-1] != ' ' && end[-1] != '\n' && end[-1] != '\r' && end[-1] != '\t');
    end[-1] = 0;
}

wchar_t *PrintI64(wchar_t *dest, uint64_t value)
{
    wchar_t *start = dest;
    do
    {
        *dest++ = '0' + (value % 10);
        value /= 10;
    }
    while (value);
    wchar_t *end = dest - 1;
    while (start < end)
    {
        wchar_t tmp = *end;
        *end = *start;
        *start = tmp;
        start++;
        end--;
    }
    return dest;
}

wchar_t *PrintI64H(wchar_t *dest, uint64_t value)
{
    wchar_t *end = dest + 16;
    while (end > dest)
    {
        uint64_t tmp = value % 16;
        *--end = (tmp < 10 ? '0' + tmp : ('A' - 10) + tmp);
        value /= 16;
    }
    return dest + 16;
}

wchar_t *Print2H(wchar_t *dest, uint8_t value)
{
    wchar_t *end = dest + 2;
    while (end > dest)
    {
        uint64_t tmp = value % 16;
        *--end = (tmp < 10 ? '0' + tmp : ('A' - 10) + tmp);
        value /= 16;
    }
    return dest + 2;
}

wchar_t buf[16*2048] = {};

void myPrintf(const wchar_t *format, ...)
{
    
    va_list args;
    va_start(args, format);

    const wchar_t *s = format;
    wchar_t *d = buf;
    
    while (*s)
    {
        if (*s == '%')
        {
            if (s[1] == '%')
            {
                *d++ = '%';
                s += 2;
            }
            else if (s[1] == 's')
            {
                const char *str = va_arg(args, const char *);
                // DWORD written;
                // int len = 0;
                // while (str[len]) len++;
                // WriteConsoleA(hOutput, str, len, &written, NULL);
                int64_t converted = MultiByteToWideChar(CP_UTF8, 0, str, -1, d, 1024*1024*1024); // no limit
                d += converted;
                s += 2;
            }
            else if (s[1] == 'l' && s[2] == 'l' && s[3] == 'd')
            {
                int64_t value = va_arg(args, int64_t);
                d = PrintI64(d, value);
                s += 4;
            }
            else if ('1' <= s[1] && s[1] <= '9' && s[2] == 'l' && s[3] == 'l' && s[4] == 'd')
            {
                int64_t value = va_arg(args, int64_t);
                wchar_t buf[32], *end;
                end = PrintI64(buf, value);
                int64_t cnt = s[1] - '0' - (end - buf);
                for (int64_t i = 0; i < cnt; ++i)
                {
                    *d++ = ' ';
                }
                memcpy(d, buf, (end - buf)*sizeof(*buf));
                d += (end - buf);
                s += 5;
            }
            else if (s[1] == 'p')
            {
                uint64_t value = va_arg(args, uint64_t);
                d = PrintI64H(d, value);
                s += 2;
            }
            else if (s[1] == '0' && s[2] == '2' && s[3] == 'x')
            {
                int64_t value = va_arg(args, int32_t);
                d = Print2H(d, value);
                s += 4;
            }
            else if (s[1] == 'l' && s[2] == 'l' && s[3] == 'x')
            {
                int64_t value = va_arg(args, int64_t);
                d = PrintI64H(d, value);
                s += 4;
            }
            else
            {
                s++;
            }
        }
        else
        {
            *d++ = *s++;
        }
    }
    
    va_end(args);

    unsigned long written;
    WriteConsoleW(hOutput, buf, d - buf, &written, NULL);
}

[[noreturn]] void assertion_failure(const char* file, int64_t line, const char* func, const char* expr) 
{
    myPrintf(L"Assertion failed: %s\n", expr);
    myPrintf(L"File: %s, Line: %lld, Function: %s\n", file, line, func);
    ExitProcess(0x0);
}

int64_t myAbs(int64_t x)
{
    return (x < 0 ? -x : x);
}

int64_t MicrosecondsToTicks(int64_t microseconds)
{
    return (frequency * microseconds) / 1000000;
}

int64_t TicksToMicroseconds(int64_t ticks)
{
    return (ticks * 1000000) / frequency;
}

int64_t GetTicks()
{
    int64_t now;
    QueryPerformanceCounter((void *)&now);
    return now;
}

int64_t SheduleTimeoutFromNow(int64_t microseconds)
{
    int64_t now;
    QueryPerformanceCounter((void *)&now);
    return now + MicrosecondsToTicks(microseconds);
}

void init_lib()
{
    QueryPerformanceFrequency((void *)&frequency);
    
    hOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    hInput = GetStdHandle(STD_INPUT_HANDLE);
    hHeap = GetProcessHeap();
}
