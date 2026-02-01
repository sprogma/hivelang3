#ifndef CODEGEN_HPP
#define CODEGEN_HPP

#include "inttypes.h"

#include "../ir.hpp"

typedef uint8_t BYTE;

class CodeAssembler
{
public:
    virtual pair<BYTE *, BYTE *> Build(BuildResult *t, BYTE *header, BYTE *body, int64_t bodyOffset) = 0;
};


CodeAssembler *new_x64_Assembler();
CodeAssembler *new_gpu_Assembler();

static inline bool validateProvider(const string& name)
{
    return name == "x64" || name == "gpu";
}

static inline bool AllowInlining(const string& name)
{
    return name == "x64";
}


enum header_id_action
{
    ACTION_PUSH_OBJECT,
    ACTION_QUERY_OBJECT,
    ACTION_PUSH_PIPE,
    ACTION_QUERY_PIPE,
    ACTION_NEW_OBJECT,
    ACTION_CAST_OBJECT,
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
            return (provider == "x64" ? 3 : 23);
        case ACTION_CAST_OBJECT:
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
}


#endif
