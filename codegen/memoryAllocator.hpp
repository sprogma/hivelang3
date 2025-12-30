#ifndef MEMORY_ALLOCATOR
#define MEMORY_ALLOCATOR

#include "../ir.hpp"

class ISpreadMemory
{
public:
    virtual pair<map<int64_t, int64_t>, int64_t> spreadMemory(WorkerDeclarationContext *wk) = 0;
};

ISpreadMemory *newSpreadMemory();

#endif
