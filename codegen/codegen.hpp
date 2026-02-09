#ifndef CODEGEN_HPP
#define CODEGEN_HPP

#include "inttypes.h"

#include "../ir.hpp"

typedef uint8_t BYTE;

class CodeAssembler
{
public:
    virtual ~CodeAssembler(){}
    virtual pair<BYTE *, BYTE *> Build(BuildResult *t, BYTE *header, BYTE *body, int64_t bodyOffset) = 0;
};


CodeAssembler *new_x64_Assembler();
CodeAssembler *new_gpu_Assembler();
CodeAssembler *new_DLL_Assembler();

int64_t GetExportWorkerId(BuildResult *ctx, int64_t wkId, const string &provider);

static inline bool validateProvider(const string& name)
{
    return name == "x64" || name == "gpu" || name == "dll";
}

static inline bool AllowInlining(const string& name)
{
    return name == "x64";
}

static inline int64_t ProviderId(const string &name)
{
    if (name == "x64")
    {
        return 0;
    }
    if (name == "gpu")
    {
        return 1;
    }
    if (name == "dll")
    {
        return 2;
    }
    return -1;
}


enum header_id_action
{
    ACTION_PUSH_OBJECT,
    ACTION_QUERY_OBJECT,
    ACTION_PUSH_PIPE,
    ACTION_QUERY_PIPE,
    ACTION_NEW_OBJECT,
    ACTION_CAST_PROVIDER,
    ACTION_CALL_WORKER,
    HEADER_DLL_IMPORT,
    HEADER_X64_WORKERS,
    HEADER_GPU_WORKERS,
    HEADER_STRINGS_TABLE,
};

static inline int8_t GetHeaderId(enum header_id_action action, const string &provider="")
{
    switch (action)
    {
        case ACTION_NEW_OBJECT:
            return (provider == "x64" ? 2 : 22);
        case ACTION_PUSH_OBJECT:
            return (provider == "x64" ? 0 : 20);
        case ACTION_QUERY_OBJECT:
            return (provider == "x64" ? 1 : 21);
        case ACTION_PUSH_PIPE:
            return (provider == "x64" ? 8 : 28);
        case ACTION_QUERY_PIPE:
            return (provider == "x64" ? 9 : 29);
        case ACTION_CALL_WORKER:
            if (provider == "x64") return 3;
            if (provider == "gpu") return 23;
            if (provider == "dll") return 33;
            break;
        case ACTION_CAST_PROVIDER:
            return 10;
        case HEADER_DLL_IMPORT:
            return 4;
        case HEADER_X64_WORKERS:
            return 16;
        case HEADER_GPU_WORKERS:
            return 18;
        case HEADER_STRINGS_TABLE:
            return 17;
    }
    printf("Error: unsupported action: %lld on provider %s\n", (int64_t)action, provider.c_str());
    return -1;
}


#endif
