#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <vector>
#include <bitset>
#include <array>
#include <ranges>
#include <map>

using namespace std;


#include "../../ir.hpp"
#include "../../optimization/optimizer.hpp"
#include "../codegen.hpp"
#include "../registerAllocator.hpp"
#include "../memoryAllocator.hpp"


enum asm_operationZ
{
    ASM_CQO,
};

enum asm_operation1
{
    ASM_NEG,
    ASM_NOT,
    
    ASM_IDIV,
    ASM_DIV,

    ASM_SETE,
    ASM_SETNE,
    ASM_SETL,
    ASM_SETLE,
    ASM_SETG,
    ASM_SETGE,
    ASM_SETA,
    ASM_SETAE,
    ASM_SETB,
    ASM_SETBE,
};

enum asm_operation2
{
    ASM_MOV,
    ASM_MOVSX,
    ASM_MOVZX,
    
    ASM_XOR,
    ASM_AND,
    ASM_OR,
    
    ASM_ADD,
    ASM_SUB,
    ASM_IMUL,
    
    ASM_TEST,
    ASM_CMP,
};

enum asm_operation3rm
{
    ASM_MOV_RM,
};

enum asm_operation3mr
{
    ASM_MOV_MR,
};

enum asm_operation3
{
    ASM_SHLX,
    ASM_SHRX,
    ASM_SARX,
};


class WinX64Assembler : public CodeAssembler
{
public:
    WinX64Assembler()
    {}

private:
    map<int64_t, WorkerDeclarationContext *> idToWorker;
    BuildResult *ir;
    BYTE *assemblyCode, *assemblyEnd;
    int64_t assemblyAlloc;
    int64_t nextLabelId;
    
    void printZ(asm_operationZ op)
    {
        AllocCode();
        switch (op)
        {
            case ASM_CQO:
            {
                *assemblyEnd++ = 0x48;
                *assemblyEnd++ = 0x99;
                break;
            }
        }
    }
    
    void printR(asm_operation1 op, pair<int64_t, int64_t> r1)
    {
        AllocCode();

        // common data
        BYTE rex = 0x40;
        bool needrex = false;

        if (r1.first & 8) { needrex = true; rex |= 0x01; }
        if (r1.second == 1) { needrex |= (r1.first & 7) >= 4; }
        if (r1.second == 8) { needrex = true; rex |= 0x08; }

        switch (op)
        {
            case ASM_NEG:
            {
                if (r1.second == 2) { *assemblyEnd++ = 0x66; }
                if (needrex) { *assemblyEnd++ = rex; }
                *assemblyEnd++ = (r1.second == 1 ? 0xF6 : 0xF7);
                *assemblyEnd++ = 0xC0 | (3 << 3) | (r1.first & 7);
                break;
            }
            case ASM_NOT:
            {
                if (r1.second == 2) { *assemblyEnd++ = 0x66; }
                if (needrex) { *assemblyEnd++ = rex; }
                *assemblyEnd++ = (r1.second == 1 ? 0xF6 : 0xF7);
                *assemblyEnd++ = 0xC0 | (2 << 3) | (r1.first & 7);
                break;
            }
            case ASM_DIV:
            {
                if (r1.second == 2) { *assemblyEnd++ = 0x66; }
                if (needrex) { *assemblyEnd++ = rex; }
                *assemblyEnd++ = (r1.second == 1 ? 0xF6 : 0xF7);
                *assemblyEnd++ = 0xC0 | (6 << 3) | (r1.first & 7);
                break;
            }
            case ASM_IDIV:
            {
                if (r1.second == 2) { *assemblyEnd++ = 0x66; }
                if (needrex) { *assemblyEnd++ = rex; }
                *assemblyEnd++ = (r1.second == 1 ? 0xF6 : 0xF7);
                *assemblyEnd++ = 0xC0 | (7 << 3) | (r1.first & 7);
                break;
            }
            case ASM_SETE:
            { 
                assert(r1.second == 1);
                if (needrex) { *assemblyEnd++ = rex; }
                *assemblyEnd++ = 0x0F;
                *assemblyEnd++ = 0x94;
                *assemblyEnd++ = 0xC0 | (0 << 3) | (r1.first & 7);
                break;
            }
            case ASM_SETNE:
            { 
                assert(r1.second == 1);
                if (needrex) { *assemblyEnd++ = rex; }
                *assemblyEnd++ = 0x0F;
                *assemblyEnd++ = 0x95;
                *assemblyEnd++ = 0xC0 | (0 << 3) | (r1.first & 7);
                break;
            }
            case ASM_SETL:
            { 
                assert(r1.second == 1);
                if (needrex) { *assemblyEnd++ = rex; }
                *assemblyEnd++ = 0x0F;
                *assemblyEnd++ = 0x9C;
                *assemblyEnd++ = 0xC0 | (0 << 3) | (r1.first & 7);
                break;
            }
            case ASM_SETLE:
            { 
                assert(r1.second == 1);
                if (needrex) { *assemblyEnd++ = rex; }
                *assemblyEnd++ = 0x0F;
                *assemblyEnd++ = 0x9E;
                *assemblyEnd++ = 0xC0 | (0 << 3) | (r1.first & 7);
                break;
            }
            case ASM_SETG:
            { 
                assert(r1.second == 1);
                if (needrex) { *assemblyEnd++ = rex; }
                *assemblyEnd++ = 0x0F;
                *assemblyEnd++ = 0x9F;
                *assemblyEnd++ = 0xC0 | (0 << 3) | (r1.first & 7);
                break;
            }
            case ASM_SETGE:
            { 
                assert(r1.second == 1);
                if (needrex) { *assemblyEnd++ = rex; }
                *assemblyEnd++ = 0x0F;
                *assemblyEnd++ = 0x9D;
                *assemblyEnd++ = 0xC0 | (0 << 3) | (r1.first & 7);
                break;
            }
            case ASM_SETA:
            { 
                assert(r1.second == 1);
                if (needrex) { *assemblyEnd++ = rex; }
                *assemblyEnd++ = 0x0F;
                *assemblyEnd++ = 0x97;
                *assemblyEnd++ = 0xC0 | (0 << 3) | (r1.first & 7);
                break;
            }
            case ASM_SETAE:
            { 
                assert(r1.second == 1);
                if (needrex) { *assemblyEnd++ = rex; }
                *assemblyEnd++ = 0x0F;
                *assemblyEnd++ = 0x93;
                *assemblyEnd++ = 0xC0 | (0 << 3) | (r1.first & 7);
                break;
            }
            case ASM_SETB:
            { 
                assert(r1.second == 1);
                if (needrex) { *assemblyEnd++ = rex; }
                *assemblyEnd++ = 0x0F;
                *assemblyEnd++ = 0x92;
                *assemblyEnd++ = 0xC0 | (0 << 3) | (r1.first & 7);
                break;
            }
            case ASM_SETBE:
            { 
                assert(r1.second == 1);
                if (needrex) { *assemblyEnd++ = rex; }
                *assemblyEnd++ = 0x0F;
                *assemblyEnd++ = 0x96;
                *assemblyEnd++ = 0xC0 | (0 << 3) | (r1.first & 7);
                break;
            }
        }
    }

    
    void printRR(asm_operation2 op, pair<int64_t, int64_t> r1, pair<int64_t, int64_t> r2)
    {
        AllocCode();

        // common data
        BYTE rex = 0x40;
        bool needrex = false;

        if (r1.first & 8) { needrex = true; rex |= 0x01; }
        if (r2.first & 8) { needrex = true; rex |= 0x04; }
        if (r1.second == 1) { needrex |= (r1.first & 7) >= 4; }
        if (r2.second == 1) { needrex |= (r2.first & 7) >= 4; }
        if (r1.second == 8) { needrex = true; rex |= 0x08; }
        if (r2.second == 8) { needrex = true; rex |= 0x08; }
        
        switch (op)
        {
            // [prefixes] [REX] [88/89] [param]
            case ASM_MOV:
            {
                assert(r1.second == r2.second);

                if (r1.second == 2) { *assemblyEnd++ = 0x66; }
                
                if (needrex) { *assemblyEnd++ = rex; }

                *assemblyEnd++ = (r1.second == 1 ? 0x88 : 0x89);
                *assemblyEnd++ = 0xC0 | ((r2.first & 7) << 3) | (r1.first & 7);
                break;
            }
            // [prefixes] [REX] [0F] [B6/B7/BE/BF] [param]        
            case ASM_MOVZX:
            case ASM_MOVSX:
            {                
                // if this fallback isn't need, don't enable it
                // if (r1.second == r2.second) return printRR(ASM_MOV, r1, r2);
                
                assert(r1.second > r2.second);

                switch (r2.second)
                {
                    case 1:
                        if (r1.second == 2) { *assemblyEnd++ = 0x66; }
                            
                        if (needrex) { *assemblyEnd++ = rex; }

                        *assemblyEnd++ = 0x0F;
                        *assemblyEnd++ = (op == ASM_MOVZX) ? 0xB6 : 0xBE;
                        *assemblyEnd++ = 0xC0 | ((r1.first & 7) << 3) | (r2.first & 7);
                        break;
                    case 2:
                        // dest is only 32/64
                        *assemblyEnd++ = 0x66;

                        if (needrex) { *assemblyEnd++ = rex; }

                        *assemblyEnd++ = 0x0F;
                        *assemblyEnd++ = (op == ASM_MOVZX) ? 0xB7 : 0xBF;
                        *assemblyEnd++ = 0xC0 | ((r1.first & 7) << 3) | (r2.first & 7);
                        break;
                    case 4:
                        // dest is only 8 bit
                        if (op == ASM_MOVZX)
                        {
                            // simple mov
                            printRR(ASM_MOV, {r1.first, 4}, r2);
                        }
                        else
                        {
                            *assemblyEnd++ = rex;
                            *assemblyEnd++ = 0x63;
                            *assemblyEnd++ = 0xC0 | ((r2.first & 7) << 3) | (r1.first & 7);
                        }
                        break;
                }
                break;
            }                
            // [prefixes] [REX] [88/89] [param]
            case ASM_XOR:
            {
                assert(r1.second == r2.second);
                if (r1.second == 2) { *assemblyEnd++ = 0x66; }
                if (needrex) { *assemblyEnd++ = rex; }
                *assemblyEnd++ = (r1.second == 1 ? 0x32 : 0x33);
                *assemblyEnd++ = 0xC0 | ((r2.first & 7) << 3) | (r1.first & 7);
                break;
            }
            case ASM_AND:
            {
                assert(r1.second == r2.second);
                if (r1.second == 2) { *assemblyEnd++ = 0x66; }
                if (needrex) { *assemblyEnd++ = rex; }
                *assemblyEnd++ = (r1.second == 1 ? 0x22 : 0x23);
                *assemblyEnd++ = 0xC0 | ((r2.first & 7) << 3) | (r1.first & 7);
                break;
            }
            case ASM_OR:
            {
                assert(r1.second == r2.second);
                if (r1.second == 2) { *assemblyEnd++ = 0x66; }
                if (needrex) { *assemblyEnd++ = rex; }
                *assemblyEnd++ = (r1.second == 1 ? 0x0A : 0x0B);
                *assemblyEnd++ = 0xC0 | ((r2.first & 7) << 3) | (r1.first & 7);
                break;
            }
            case ASM_ADD:
            {
                assert(r1.second == r2.second);
                if (r1.second == 2) { *assemblyEnd++ = 0x66; }
                if (needrex) { *assemblyEnd++ = rex; }
                *assemblyEnd++ = (r1.second == 1 ? 0x02 : 0x03);
                *assemblyEnd++ = 0xC0 | ((r2.first & 7) << 3) | (r1.first & 7);
                break;
            }
            case ASM_SUB:
            {
                assert(r1.second == r2.second);
                if (r1.second == 2) { *assemblyEnd++ = 0x66; }
                if (needrex) { *assemblyEnd++ = rex; }
                *assemblyEnd++ = (r1.second == 1 ? 0x2A : 0x2B);
                *assemblyEnd++ = 0xC0 | ((r2.first & 7) << 3) | (r1.first & 7);
                break;
            }
            case ASM_IMUL:
            {
                assert(r1.second == r2.second);
                if (r1.second <= 2) { *assemblyEnd++ = 0x66; } // multiplicate 8 byte as 16 byte
                if (needrex) { *assemblyEnd++ = rex; }
                *assemblyEnd++ = 0x0F;
                *assemblyEnd++ = 0xAF;
                *assemblyEnd++ = 0xC0 | ((r2.first & 7) << 3) | (r1.first & 7);
                break;
            }
            case ASM_TEST:
            {
                assert(r1.second == r2.second);
                if (r1.second == 2) { *assemblyEnd++ = 0x66; }
                if (needrex) { *assemblyEnd++ = rex; }
                *assemblyEnd++ = (r1.second == 1 ? 0x84 : 0x85);
                *assemblyEnd++ = 0xC0 | ((r2.first & 7) << 3) | (r1.first & 7);
                break;
            }
            case ASM_CMP:
            {
                assert(r1.second == r2.second);
                if (r1.second == 2) { *assemblyEnd++ = 0x66; }
                if (needrex) { *assemblyEnd++ = rex; }
                *assemblyEnd++ = (r1.second == 1 ? 0x3A : 0x3B);
                *assemblyEnd++ = 0xC0 | ((r2.first & 7) << 3) | (r1.first & 7);
                break;
            }
        }
    }

    void printRM(asm_operation3rm op, pair<int64_t, int64_t> r1, pair<int64_t, int64_t> r2, int64_t offset)
    {
        AllocCode();
        
        BYTE rex = 0x40;
        bool needrex = false;
        
        if (r1.first & 8) { needrex = true; rex |= 0x04; }
        if (r2.first & 8) { needrex = true; rex |= 0x01; }
        if (r1.second == 8) { needrex = true; rex |= 0x08; }

        switch (op)
        {
            case ASM_MOV_RM:
            {   
                assert(r2.second == 8);

                // add 16 bit prefix
                if (r1.second == 2)  { *assemblyEnd++ = 0x66; }

                // add rex
                if (needrex)  { *assemblyEnd++ = rex; }

                // opcode
                *assemblyEnd++ = (r1.second == 1 ? 0x8A : 0x8B);

                // modrm
                if (offset == 0 && (r2.first & 7) != 5) // if not rbp/r13
                {
                    *assemblyEnd++ = 0x00 | ((r1.first & 7) << 3) | (r2.first & 7);
                } 
                else if (offset >= INT8_MIN && offset <= INT8_MAX) 
                {
                    *assemblyEnd++ = 0x40 | ((r1.first & 7) << 3) | (r2.first & 7);
                } 
                else 
                {
                    *assemblyEnd++ = 0x80 | ((r1.first & 7) << 3) | (r2.first & 7);
                }
                
                if ((r2.first & 7) == 4)  // if rsp/r12
                {
                    // forced to use SIB
                    *assemblyEnd++ = (0x00 << 6) | (0x04 << 3) | (r2.first & 7);
                } 

                // offset
                if (offset || (r2.first & 7) == 5) // if rbp/r13 - write offset even if it is 0
                {
                    if (offset >= INT8_MIN && offset <= INT8_MAX) 
                    {
                        *assemblyEnd++ = offset;
                    } 
                    else 
                    {
                        uint32_t data = offset;
                        memcpy(assemblyEnd, &data, sizeof(data));
                        assemblyEnd += sizeof(data);
                    }
                }
            }
        }
    }

    void printMR(asm_operation3mr op, pair<int64_t, int64_t> r1, pair<int64_t, int64_t> r2, int64_t offset)
    {
        AllocCode();
        
        BYTE rex = 0x40;
        bool needrex = false;
        
        if (r1.first & 8) { needrex = true; rex |= 0x01; }
        if (r2.first & 8) { needrex = true; rex |= 0x04; }
        if (r2.second == 8) { needrex = true; rex |= 0x08; }

        switch (op)
        {
            case ASM_MOV_MR:
            {   
                assert(r1.second == 8);

                if (r2.second == 2)  { *assemblyEnd++ = 0x66; }

                // rex
                if (needrex)  { *assemblyEnd++ = rex; }

                // opcode
                *assemblyEnd++ = (r2.second == 1 ? 0x88 : 0x89);

                // modrm
                if (offset == 0 && (r1.first & 7) != 5) // if not rbp/r13
                {
                    *assemblyEnd++ = 0x00 | ((r2.first & 7) << 3) | (r1.first & 7);
                } 
                else if (offset >= INT8_MIN && offset <= INT8_MAX) 
                {
                    *assemblyEnd++ = 0x40 | ((r2.first & 7) << 3) | (r1.first & 7);
                } 
                else 
                {
                    *assemblyEnd++ = 0x80 | ((r2.first & 7) << 3) | (r1.first & 7);
                }
                
                if ((r1.first & 7) == 4)  // if rsp/r12
                {
                    // forced to use SIB
                    *assemblyEnd++ = (0x00 << 6) | (0x04 << 3) | (r1.first & 7);
                } 

                // offset
                if (offset || (r1.first & 7) == 5) // if rbp/r13 - write offset even if zero
                {
                    if (offset >= INT8_MIN && offset <= INT8_MAX) 
                    {
                        *assemblyEnd++ = offset;
                    } 
                    else 
                    {
                        uint32_t data = offset;
                        memcpy(assemblyEnd, &data, sizeof(data));
                        assemblyEnd += sizeof(data);
                    }
                }
                break;
            }
        }
    }

    void printRRR(asm_operation3 op, pair<int64_t, int64_t> r1, pair<int64_t, int64_t> r2, pair<int64_t, int64_t> r3)
    {
        AllocCode();

        assert(r1.second == r2.second && r2.second == r3.second);
        assert(r1.second == 4 || r1.second == 8);

        switch (op)
        {
            case ASM_SHLX:
            case ASM_SHRX:
            case ASM_SARX:
            {
                // generate VEX
                BYTE pp;
                switch (op) {
                    case ASM_SHLX: pp = 0x01; break; // 66 prefix
                    case ASM_SHRX: pp = 0x03; break; // F2 prefix  
                    case ASM_SARX: pp = 0x02; break; // F3 prefix
                }
                
                *assemblyEnd++ = 0xC4;
                
                BYTE r_bit = !!(r1.first & 8);
                BYTE x_bit = 1;
                BYTE b_bit = !!(r3.first & 8);
                BYTE map_select = 0x02; // can't use C5 vex
                
                *assemblyEnd++ = (r_bit << 7) | (x_bit << 6) | (b_bit << 5) | map_select;
                
                BYTE W = (r1.second == 8) ? 1 : 0;
                BYTE vvvv = (~r2.first) << 3;
                BYTE L = 0;
                
                *assemblyEnd++ = (W << 7) | vvvv | (L << 2) | pp;

                // modrm
                *assemblyEnd++ = 0xF7;
                
                BYTE modrm = 0xC0 | ((r1.first & 7) << 3) | (r3.first & 7);
                *assemblyEnd++ = modrm;
                break;
            }
        }
    }

    void AllocCode()
    {
        int64_t pos = assemblyEnd - assemblyCode;
        if (pos + 1000 > assemblyAlloc)
        {
            while (pos + 1000 > assemblyAlloc)
            {
                assemblyAlloc = 2 * assemblyAlloc + !assemblyAlloc;
            }
            assemblyCode = (BYTE *)realloc(assemblyCode, assemblyAlloc);
            assemblyEnd = assemblyCode + pos;
        }
    }
    
    __attribute__ ((format (printf, 2, 3)))
    void print(const char *format_string, ...)
    {
        va_list args;
        va_start(args, format_string); 
        vprintf(format_string, args);
        va_end(args);
    }
    
public:
    void Build(BuildResult *input, const char *resultFileName) override 
    {
        ir = input;
        printf("Building into %s\n", resultFileName);

        /* init build context */
        nextLabelId = 0;
        generated.clear();

        /* initializate string */
        assemblyAlloc = 0;
        assemblyCode = assemblyEnd = NULL;

        /* build each worker */
        for (auto &[fn, id] : ir->workers) { idToWorker[id] = fn; }
        for (auto &[fn, id] : ir->workers) { BuildFn(fn, id); }

        // add terminating zero
        printf("Code generated.\n");
        if (!assemblyEnd)
        {
            return;
        }

        for (BYTE *x = assemblyCode; x < assemblyEnd; ++x)
        {
            printf(" %02X", *x);
        }
        printf("\n");
    }

private:
    WorkerDeclarationContext *current;
    // registers table
    map<int64_t, int64_t> regTable;
    map<int64_t, int64_t> memTable;
    int64_t usedMemory;
    set<OperationBlock *> generated;
    set<OperationBlock *> usedLabels;

    // registers configuraion
    static constexpr int64_t registersCount = 5;

    bool isSigned(int64_t name)
    {
        return SCALAR_TYPE(varType(name)->type) == SCALAR_I;
    }

    TypeContext *varType(int64_t name)
    {
        return current->content->variables[name];
    }

    int64_t varSize(int64_t name)
    {
        return current->content->variables[name]->size;
    }

    int64_t GetInputOffset(int64_t id)
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

    int64_t GetOutputOffset(int64_t id)
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

    const char *SizeQualifer(TypeContext *var) {
        switch (var->size) {
            case 8: return "QWORD PTR";
            case 4: return "DWORD PTR";
            case 2: return "WORD PTR";
            case 1: return "BYTE PTR";
        } 
        printf("Wrong variable size - there is no qualifier of size %lld\n", var->size);
        return "ERROR PTR";
    }

    const char *SizeQualifer(int64_t var) {
        switch (varSize(var)) {
            case 8: return "QWORD PTR";
            case 4: return "DWORD PTR";
            case 2: return "WORD PTR";
            case 1: return "BYTE PTR";
        } 
        printf("Wrong variable size - there is no qualifier of size %lld\n", varSize(var));
        return "ERROR PTR";
    }

    const char *RegisterName(int64_t var)
    {
        return RegisterName(regTable[var], varSize(var));
    }

    const int64_t registers[5] = {0b0001, 0b1000, 0b1001, 0b1010, 0b1011};

    const pair<int64_t, int64_t> Register(int64_t var)
    {
        return {registers[regTable[var]], varSize(var)};
    }

    /*
        registers: 
            rbp - pointer on locals
            rdi - pointer on inputs table // TODO: optimize
            rax - used for division / api calls
            rdx - used for division / api calls
    */
    
    const char *RegisterName(int64_t id, int64_t size)
    {
        switch (size)
        {
            case 8: return (vector<const char *>{"rcx",  "r10",  "r11",  "r12",  "r13"})[id];
            case 4: return (vector<const char *>{"ecx", "r10d", "r11d", "r12d", "r13d"})[id];
            case 2: return (vector<const char *>{ "cx", "r10w", "r11w", "r12w", "r13w"})[id];
            case 1: return (vector<const char *>{ "cl", "r10b", "r11b", "r12b", "r13b"})[id];
        }
        printf("Wrong variable size - there is no register of size %lld\n", size);
        return "ERROR";
    }

    void ApiCall(const char *apiEntry)
    {
        print("\tjmp  %s\n", apiEntry);
    }

    void BuildFn(WorkerDeclarationContext *wk, int64_t workerId)
    {
        current = wk;

        // create registers translation
        {
            SpreadRegisters<registersCount> regSprd;
            regTable = regSprd.spreadRegisters(wk);
        }

        // allocate memory
        {
            ISpreadMemory *memSprd = newSpreadMemory();
            const auto &[tbl, sz] = memSprd->spreadMemory(wk);
            memTable = tbl;
            usedMemory = sz;
        }

        // print memory table
        for (auto &[k, v] : memTable)
        {
            printf("Var %lld have offset %lld\n", k, v);
        }
        printf("Total size of variables: %lld\n", usedMemory);

        dumpIR(wk);

        // get used labels
        generated.clear();
        usedLabels.clear();
        UpdateUsedLabels(wk->content->entry);
        
        // generate code
        
        print("worker_%lld:\n", workerId);
        if (wk->content == NULL)
        {
            printf("For now, x64-win doesn't support dynamic linking\n");
        }
        else
        {
            generated.clear();
            BuildOperation(wk->content->entry);
        }
    }

    void UpdateUsedLabels(OperationBlock *op)
    {
        if (op == NULL) return;
        if (!generated.insert(op).second) { usedLabels.insert(op); return; }
        if (op->next.size() > 0)
        {
            UpdateUsedLabels(op->next[0]);
            for (auto &n : op->next | views::drop(1)) { UpdateUsedLabels(n); usedLabels.insert(n); }
        }
    }

    void InsertMove(pair<int64_t, int64_t> dest, pair<int64_t, int64_t> from, bool isSigned)
    {
        if (dest.second <= from.second)
        {
            if (dest.first != from.first)
            {
                printRR(ASM_MOV, dest, {from.first, dest.second});
            }
            return;
        }
        printRR((isSigned ? ASM_MOVSX : ASM_MOVZX), dest, from);
    }

    void InsertMove(int64_t dest, int64_t from)
    {
        InsertMove(Register(dest), Register(from), isSigned(dest));
    }

    void InsertInteger(int64_t dest, int64_t value)
    {
        switch (value)
        {
            case 0:
                printRR(ASM_XOR, Register(dest), Register(dest));
                break;
            default:
                print("\tmov  %s, %lld\n", RegisterName(dest), value);
                break;
        }
    }

    void BuildOperation(OperationBlock *op)
    {
        // return from function
        if (op == NULL)
        {
            print("\tret ; ...\n");
            return;
        }
        // if already this block is compiled - jump to it
        // TODO: here can take some place of "assembly inlining"
        if (!generated.insert(op).second) 
        { 
            print("\tjmp  op_%p\n", op); 
            return; 
        }

        // generate code for this instruction        
        if (usedLabels.contains(op))
        {
            print("op_%p:\n", op);
        }
        // if instruction have many next:
        #define ABEL_BINOP(T) \
            if (regTable[op->data[0]] == regTable[op->data[2]]) \
            { \
                printRR(T, Register(op->data[2]), Register(op->data[1])); \
            } \
            else \
            { \
                InsertMove(op->data[0], op->data[1]); \
                printRR(T, Register(op->data[0]), Register(op->data[2])); \
            }
        #define CMPOP(A, B) { \
            printRR(ASM_CMP, Register(op->data[1]), Register(op->data[2])); \
            print("\tmov  %s, 0\n", RegisterName(op->data[0])); \
            auto reg = Register(op->data[0]); \
            reg.second = 1; \
            if (isSigned(op->data[0])) \
                printR(A, reg); \
            else \
                printR(B, reg); \
            printR(ASM_NEG, Register(op->data[0])); \
        }
        
        switch (op->type)
        {
            // impossible
            case OP_JMP: break;    
            // nothing to do
            case OP_FREE_TEMP: break;
            
            case OP_JZ:
                printRR(ASM_TEST, Register(op->data[0]), Register(op->data[0]));
                print("\tjz   op_%p\n", op->next[1]);
                break;
                
            case OP_JNZ: 
                printRR(ASM_TEST, Register(op->data[0]), Register(op->data[0]));
                print("\tjnz  op_%p\n", op->next[1]);
                break;
        
            case OP_LOAD:
                // mov $0, XX PTR [rbp + $1]
                printRM(ASM_MOV_RM, Register(op->data[0]), {5, 8}, memTable[op->data[1]]);
                break;
            case OP_STORE:
                // mov XX PTR [rbp + $1], $0
                printMR(ASM_MOV_MR, {5, 8}, Register(op->data[0]), memTable[op->data[1]]);
                break;
        
            case OP_LOAD_INPUT: 
                // mov $0, XX PTR [rdi + $1]
                printRM(ASM_MOV_RM, Register(op->data[1]), {7, 8}, GetInputOffset(op->data[0]));
                break;
            case OP_LOAD_OUTPUT: 
                // mov $0, XX PTR [rdi + $1]
                printRM(ASM_MOV_RM, Register(op->data[1]), {7, 8}, GetOutputOffset(op->data[0]));
                break;
            
            
            case OP_CALL: 
                print("OP_CALL [not supported]\n"); 
                break;
                
            case OP_CAST: 
                InsertMove(op->data[0], op->data[1]);
                break;
                
            case OP_MOV: 
                InsertMove(op->data[0], op->data[1]);
                break;

            case OP_NEW_INT: 
                InsertInteger(op->data[0], op->data[1]);
                break;
                
            case OP_NEW_FLOAT:
                print("\tOP_NEW_FLOAT [not supported]\n"); 
                break;
                
            case OP_NEW_ARRAY:    ApiCall("new_array"); break;
            case OP_NEW_PIPE:     ApiCall("new_pipe"); break;
            case OP_NEW_PROMISE:  ApiCall("new_promise"); break;
            case OP_NEW_CLASS:    ApiCall("new_class"); break;
            
            case OP_PUSH_VAR:
                assert(op->data[1] != 0 || (TypeContext *)op->data[2] != varType(op->data[0]));
                // mov XX PTR [rbp + $0 + $1], $3
                printMR(ASM_MOV_MR, {5, 8}, Register(op->data[3]), memTable[op->data[0]] + op->data[1]);
                // (TypeContext *)op->data[2] - type [unused for now]
                break;
                
            case OP_PUSH_ARRAY:   ApiCall("push_array"); break;
            case OP_PUSH_PIPE:    ApiCall("push_pipe"); break;
            case OP_PUSH_PROMISE: ApiCall("push_promise"); break;
            case OP_PUSH_CLASS:   ApiCall("push_class"); break;
                
            case OP_QUERY_VAR: 
                assert(op->data[2] != 0 || (TypeContext *)op->data[3] != varType(op->data[0]));
                // mov $0, XX PTR [rbp + $1 + $2]
                printRM(ASM_MOV_RM, Register(op->data[0]), {5, 8}, memTable[op->data[1]] + op->data[2]);
                // (TypeContext *)op->data[3] - type [unused for now]
                break;
                
            case OP_QUERY_ARRAY:   ApiCall("query_array"); break;
            case OP_QUERY_INDEX:   ApiCall("query_index"); break;
            case OP_QUERY_PIPE:    ApiCall("query_pipe"); break;
            case OP_QUERY_PROMISE: ApiCall("query_promise"); break;
            case OP_QUERY_CLASS:   ApiCall("query_class"); break;
             
            case OP_BOR:   ABEL_BINOP(ASM_OR) break;
            case OP_BAND:  ABEL_BINOP(ASM_AND) break;
            case OP_BXOR:  ABEL_BINOP(ASM_XOR) break;

            // TODO: add variant without BMI2
            case OP_SHL:   
                // shlx $0 $1 $2
                printRRR(ASM_SHLX, Register(op->data[0]), Register(op->data[1]), Register(op->data[2]));
                break;
            case OP_SHR:
                // shrx $0 $1 $2
                printRRR(ASM_SHRX, Register(op->data[0]), Register(op->data[1]), Register(op->data[2]));
                break;
            
            case OP_BNOT:
                InsertMove(op->data[0], op->data[2]);
                printR(ASM_NOT, Register(op->data[0]));
                break;
            
            case OP_ADD:   ABEL_BINOP(ASM_ADD) break;
            case OP_MUL:   ABEL_BINOP(ASM_IMUL) break;
            case OP_SUB:
                if (regTable[op->data[0]] == regTable[op->data[2]])
                {
                    printRR(ASM_SUB, Register(op->data[0]), Register(op->data[1]));
                    printR(ASM_NEG, Register(op->data[0]));
                }
                else
                {
                    InsertMove(op->data[0], op->data[1]);
                    printRR(ASM_SUB, Register(op->data[0]), Register(op->data[2]));
                }
                break;
            
            case OP_EQ:    CMPOP(ASM_SETE,  ASM_SETE)  break;
            case OP_NE:    CMPOP(ASM_SETNE, ASM_SETNE) break;
            case OP_LT:    CMPOP(ASM_SETL,  ASM_SETB)  break;
            case OP_LE:    CMPOP(ASM_SETLE, ASM_SETBE) break;
            case OP_GT:    CMPOP(ASM_SETG,  ASM_SETA)  break;
            case OP_GE:    CMPOP(ASM_SETGE, ASM_SETAE) break;

            case OP_DIV:
                InsertMove({0, 8}, Register(op->data[1]), isSigned(op->data[0]));            // mov rax, $1
                if (isSigned(op->data[0])) 
                    printZ(ASM_CQO);                                                         // cqo
                else 
                    printRR(ASM_XOR, {2, 4}, {2, 4});                                        // xor edx, edx
                printR((isSigned(op->data[0]) ? ASM_IDIV : ASM_DIV), Register(op->data[2])); // div $2
                InsertMove(Register(op->data[0]), {0, 8}, false);                            // mov $0, rax ; sign doesn't matter
                break;
                
            case OP_MOD:
                InsertMove({0, 8}, Register(op->data[1]), isSigned(op->data[0]));            // mov rax, $1
                if (isSigned(op->data[0])) 
                    printZ(ASM_CQO);                                                         // cqo
                else 
                    printRR(ASM_XOR, {2, 4}, {2, 4});                                        // xor edx, edx
                printR((isSigned(op->data[0]) ? ASM_IDIV : ASM_DIV), Register(op->data[2])); // div $2
                InsertMove(Register(op->data[0]), {2, 8}, false);                            // mov $0, rdx ; sign doesn't matter
                break;
        }

        for (auto &n : op->next)
        {
            BuildOperation(n);
        }
    }
};


CodeAssembler *new_x64_win_Assembler()
{
    return new WinX64Assembler();
}
