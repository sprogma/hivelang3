#ifndef CODEGEN_HPP
#define CODEGEN_HPP

#include "inttypes.h"

#include "../ir.hpp"

typedef uint8_t BYTE;

class CodeAssembler
{
public:
    virtual void Build(BuildResult *t, const char *resultFileName) = 0;
};


CodeAssembler *new_x64_win_Assembler();


#endif
