#include "codegen.hpp"

CodeAssembler *new_x64_Assembler(const std::map<std::string, std::string> &config)
{
    return new WinX64Assembler(config);
}
