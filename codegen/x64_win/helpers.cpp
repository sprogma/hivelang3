#include "codegen.hpp"


bool WinX64Assembler::isSigned(int64_t name)
{
    return SCALAR_TYPE(varType(name)->_scalar.kind) == SCALAR_I;
}

bool WinX64Assembler::isApiScalar(int64_t name)
{
    return (varType(name)->type != TYPE_RECORD && varType(name)->type != TYPE_UNION);
}

TypeContext *WinX64Assembler::varType(int64_t name)
{
    return current->content->variables[name];
}

int64_t WinX64Assembler::varSize(int64_t name)
{
    return current->content->variables[name]->size;
}

int64_t WinX64Assembler::varTypeID(TypeContext *type)
{
    //<<--Quote-->> from:../../runtimes/0.3/win/runtime.h:(^|(?<=\n)) *#define\s+OBJECT_\w+\s+0x[\da-fA-F]+
    // #define OBJECT_PIPE           0x01
    // #define OBJECT_PROMISE        0x02
    // #define OBJECT_ARRAY          0x03
    // #define OBJECT_OBJECT         0x04
    // #define OBJECT_DEFINED_ARRAY  0x05
    //<<--QuoteEnd-->>
    switch (type->type)
    {
        case TYPE_PIPE:
            return 0x01;
        case TYPE_PROMISE:
            return 0x02;
        case TYPE_ARRAY:
            return 0x03;
        case TYPE_CLASS:
            return 0x04;
        default:
            return 0;
    }
}

int64_t WinX64Assembler::GetInputOffset(int64_t id)
{
    // sum all sizes from inputs and outputs
    int64_t res = 0;
    for (auto &[name, type] : current->inputs)
    {
        if (!id--) break;
        res += type->size;
    }
    return res;
}

int64_t WinX64Assembler::GetWorkerInputTableSize(int64_t id)
{
    int64_t res = 0;
    
    for (auto &[name, type] : array{views::all(idToWorker[id]->inputs), 
                                    views::all(idToWorker[id]->outputs)} | views::join)
    {
        res += type->size;
    }
    return res;
}

int64_t WinX64Assembler::GetOutputOffset(int64_t id)
{
    // sum all sizes from inputs and outputs
    int64_t res = 0;
    for (auto &[name, type] : current->inputs) 
        res += type->size;
    for (auto &[name, type] : current->outputs)
    {
        if (!id--) break;
        res += type->size;
    }
    return res;
}

const pair<int64_t, int64_t> WinX64Assembler::Register(int64_t var)
{
    return {registers[regTable[var]], varSize(var)};
}

const pair<int64_t, int64_t> WinX64Assembler::Register(int64_t var, int64_t size)
{
    return {registers[regTable[var]], size};
}


void WinX64Assembler::InsertMove(OperationBlock *node, pair<int64_t, int64_t> dest, pair<int64_t, int64_t> from, bool isSigned)
{
    if (dest.second <= from.second)
    {
        if (dest.first != from.first)
        {
            printRR(node, ASM_MOV, dest, {from.first, dest.second});
        }
        return;
    }
    printRR(node, (isSigned ? ASM_MOVSX : ASM_MOVZX), dest, from);
}

void WinX64Assembler::InsertMove(OperationBlock *node, int64_t dest, int64_t from)
{
    InsertMove(node, Register(dest), Register(from), isSigned(dest));
}

void WinX64Assembler::InsertInteger(OperationBlock *node, pair<int64_t, int64_t> dest, int64_t value)
{
    switch (value)
    {
        case 0:
            printRR(node, ASM_XOR, dest, dest);
            break;
        default:
            printRC(node, ASM_MOV_RC, dest, value);
            break;
    }
}

void WinX64Assembler::ExternTo64Bit(OperationBlock *node, pair<int64_t, int64_t> reg, bool is_signed)
{
    switch (reg.second)
    {
        case 1:
        case 2:
            printRR(node, (is_signed ? ASM_MOVSX : ASM_MOVZX), {reg.first, 8}, reg);
            break;
        case 4:
            if (is_signed)
            {
                printRR(node, ASM_MOVSX, {reg.first, 8}, reg);
            }
            else
            {
                // already ok [top part is cleared after any instruction]
            }
            break;
        case 8:
            // already casted
            break;
    }
}

int64_t WinX64Assembler::GetClassSize(TypeContext *type)
{
    int64_t sum = 0;
    for (auto &i : type->_struct.fields)
    {
        sum += i->size;
    }
    return sum;
}

