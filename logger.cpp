#include <algorithm>
#include <stdio.h>

using namespace std;

#include "logger.hpp"


void logError(const char *filename, char *content, int64_t start, int64_t end)
{
    logError(filename, content, (start + end) / 2);
}


void logError(const char *filename, char *content, int64_t position)
{
    char *prev_newline = content + position;
    while (prev_newline > content && prev_newline[-1] != '\n') { prev_newline--; }
    char *next_newline = strchr(content + position, '\n') ?: content + strlen(content);
    int64_t line = count(content, content + position, '\n');
    int64_t col = (content + position) - prev_newline;
    int64_t len = printf("Error: near %s:%lld:%lld> %.*s\n", filename, 
                                                            line, 
                                                            col+1, 
                                                            (int)(next_newline - prev_newline),
                                                            prev_newline);
    for (int64_t i = 0; i < len + col - (next_newline - prev_newline) - 1; ++i) { putchar(' '); }
    printf("^\n");
}


