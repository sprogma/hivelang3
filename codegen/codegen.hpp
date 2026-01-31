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


#endif
