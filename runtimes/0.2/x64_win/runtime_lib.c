#define UNICODE 1
#define _UNICODE 1

#include "windows.h"
#include "stdarg.h"
#include "inttypes.h"

#include "runtime_lib.h"

// for generated
void *memset(void *_dst, int value, size_t size)
{
    BYTE *dst = _dst;
    while (size--)
    {
        *dst++ = value;
    }
    return _dst;
}


HANDLE hOutput;
HANDLE hInput;

int64_t frequency;

BYTE *data_buffer, *data_buffer_end;
int64_t commited;
 // 64MB
#define MAX_MEMORY (1024*1024*1024)
#define COMMIT_BLOCK_SIZE (4*4096) 


void *myMalloc(int64_t size)
{
    data_buffer_end += (8 - (data_buffer_end - data_buffer) % 8) % 8;
    void *res = data_buffer_end;
    data_buffer_end += size;
    while (data_buffer_end - data_buffer > commited)
    {
        VirtualAlloc(data_buffer + commited, COMMIT_BLOCK_SIZE, MEM_COMMIT, PAGE_READWRITE);
        commited += COMMIT_BLOCK_SIZE;
    }
    return res;
}

void myFree(void *mem)
{
    (void)mem;
}


void *memcpy(void *_dst, const void *_src, size_t size)
{
    BYTE *dst = _dst, *src = (void *)_src;
    while (size--)
    {
        *dst++ = *src++;
    }
    return dst;
}

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

wchar_t buf[2048] = {};

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
                int64_t converted = MultiByteToWideChar(CP_UTF8, 0, str, -1, d, 1024*1024); // no limit
                d += converted;
                s += 2;
            }
            else if (s[1] == 'l' && s[2] == 'l' && s[3] == 'd')
            {
                int64_t value = va_arg(args, int64_t);
                d = PrintI64(d, value);
                s += 4;
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

int64_t SheduleTimeoutFromNow(int64_t microseconds)
{
    int64_t now;
    QueryPerformanceCounter((void *)&now);
    return now + (frequency * microseconds) / 1000000;
}

void init_lib()
{
    data_buffer = data_buffer_end = VirtualAlloc(NULL, MAX_MEMORY, MEM_RESERVE, PAGE_READWRITE);
    commited = COMMIT_BLOCK_SIZE;
    VirtualAlloc(data_buffer, COMMIT_BLOCK_SIZE, MEM_COMMIT, PAGE_READWRITE);

    QueryPerformanceFrequency((void *)&frequency);
    
    hOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    hInput = GetStdHandle(STD_INPUT_HANDLE);
}
