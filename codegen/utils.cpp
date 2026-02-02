#include <map>

#include "../ir.hpp"
#include "../logger.hpp"
#include "../optimization/optimizer.hpp"
#include "../analysis/analizators.hpp"
#include "codegen.hpp"
#include "registerAllocator.hpp"
#include "memoryAllocator.hpp"

int64_t GetExportWorkerId(struct BuildResult *ctx, int64_t wkId, const string &provider)
{
    if (!ctx->exportWorkerId.contains({wkId, provider}))
    {
        ctx->exportWorkerId[{wkId, provider}] = ctx->exportWorkerId.size();
    }
    return ctx->exportWorkerId[{wkId, provider}];
}
