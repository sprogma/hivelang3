#include <algorithm>
#include <stdarg.h>
#include <stdio.h>

using namespace std;

#include "logger.hpp"


void vlogError(const char *filename, const char *content, int64_t start, int64_t end, const char *format, va_list args)
{
    int64_t position = (start + end) / 2;
    const char *prev_newline = content + position;
    while (prev_newline > content && prev_newline[-1] != '\n') { prev_newline--; }
    const char *next_newline = strchr(content + position, '\n') ?: content + strlen(content);
    int64_t line = count(content, content + position, '\n');
    int64_t col = (content + position) - prev_newline;
    
    fprintf(stderr, "Error: near %s:%lld:%lld> ", filename, line + 1, col + 1);
    vfprintf(stderr, format, args);

    putc('\n', stderr); 
    int64_t len = fprintf(stderr, "at line> %.*s", (int)(next_newline - prev_newline), prev_newline);
    putc('\n', stderr); 
    for (int64_t i = 0; i < len - (next_newline - prev_newline); ++i)
    {
        putc(' ', stderr); 
    }
    for (int64_t i = prev_newline - content; i < next_newline - content - 1; ++i) 
    { 
        if (i == start)
        {
            putc('^', stderr); 
        }
        else if (i > start && i < end)
        {
            putc('~', stderr); 
        }
        else if (i < start)
        {
            putc(' ', stderr); 
        }
    }
    putc('\n', stderr); 
}

void logError(const char *filename, const char *content, int64_t start, int64_t end, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vlogError(filename, content, start, end, format, args);
    va_end(args);
}


void logError(const char *filename, const char *content, int64_t position, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vlogError(filename, content, position, position + 1, format, args);
    va_end(args);
}

