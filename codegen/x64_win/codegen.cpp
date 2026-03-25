#include "codegen.hpp"

CodeAssembler *new_x64_Assembler()
{
    return new WinX64Assembler();
}
