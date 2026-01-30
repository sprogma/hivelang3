#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <vector>
#include <algorithm>
#include <array>
#include <ranges>
#include <map>

using namespace std;


#include "../../ir.hpp"
#include "../../logger.hpp"
#include "../../optimization/optimizer.hpp"
#include "../../analysis/analizators.hpp"
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

enum asm_operation2rc
{
    ASM_MOV_RC,
    ASM_ADD_RC,
};

enum asm_operation3mr
{
    ASM_MOV_MR,
};

enum asm_operation3rrc
{
    ASM_IMUL_RRC,
};

enum asm_operation3
{
    ASM_SHLX,
    ASM_SHRX,
    ASM_SARX,
};


enum asm_operation_jmp
{
    ASM_JMP,
    ASM_JZ,
    ASM_JNZ,
    ASM_JS,
    ASM_JNS,
    ASM_JNAE,
    ASM_JB,
    ASM_JC,
    ASM_NC,
    ASM_NB,
    ASM_JAE,
    ASM_JO,
    ASM_JNO,
};

struct jmpInstruction
{
    asm_operation_jmp jmpType;
    BYTE *codePos;
    int64_t codeOrder;
    OperationBlock *destOp;
    OperationBlock *node;
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

    #define assertExpr(block, expr) \
        if (!(expr)) logError(ir->filename, ir->code, (block)->code_start, (block)->code_end, "Assertation failed: [%s:%d] %s", __FUNCTION__, __LINE__,  #expr);
    
    template<typename... Args>
    void pbyte(Args... args) 
    {
        ((*assemblyEnd++ = args), ...);
    }
    
    void printZ(OperationBlock *node, asm_operationZ op)
    {
        (void)node;
        
        switch (op)
        {
            case ASM_CQO:
            {
                pbyte(0x48);
                pbyte(0x99);
                break;
            }
        }
    }
    
    void printR(OperationBlock *node, asm_operation1 op, pair<int64_t, int64_t> r1)
    {

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
                if (r1.second == 2) { pbyte(0x66); }
                if (needrex) { pbyte(rex); }
                pbyte((r1.second == 1 ? 0xF6 : 0xF7));
                pbyte(0xC0 | (3 << 3) | (r1.first & 7));
                break;
            }
            case ASM_NOT:
            {
                if (r1.second == 2) { pbyte(0x66); }
                if (needrex) { pbyte(rex); }
                pbyte((r1.second == 1 ? 0xF6 : 0xF7));
                pbyte(0xC0 | (2 << 3) | (r1.first & 7));
                break;
            }
            case ASM_DIV:
            {
                if (r1.second == 2) { pbyte(0x66); }
                if (needrex) { pbyte(rex); }
                pbyte((r1.second == 1 ? 0xF6 : 0xF7));
                pbyte(0xC0 | (6 << 3) | (r1.first & 7));
                break;
            }
            case ASM_IDIV:
            {
                if (r1.second == 2) { pbyte(0x66); }
                if (needrex) { pbyte(rex); }
                pbyte((r1.second == 1 ? 0xF6 : 0xF7));
                pbyte(0xC0 | (7 << 3) | (r1.first & 7));
                break;
            }
            case ASM_SETE:
            { 
                assertExpr(node, r1.second == 1);
                if (needrex) { pbyte(rex); }
                pbyte(0x0F);
                pbyte(0x94);
                pbyte(0xC0 | (0 << 3) | (r1.first & 7));
                break;
            }
            case ASM_SETNE:
            { 
                assertExpr(node, r1.second == 1);
                if (needrex) { pbyte(rex); }
                pbyte(0x0F);
                pbyte(0x95);
                pbyte(0xC0 | (0 << 3) | (r1.first & 7));
                break;
            }
            case ASM_SETL:
            { 
                assertExpr(node, r1.second == 1);
                if (needrex) { pbyte(rex); }
                pbyte(0x0F);
                pbyte(0x9C);
                pbyte(0xC0 | (0 << 3) | (r1.first & 7));
                break;
            }
            case ASM_SETLE:
            { 
                assertExpr(node, r1.second == 1);
                if (needrex) { pbyte(rex); }
                pbyte(0x0F);
                pbyte(0x9E);
                pbyte(0xC0 | (0 << 3) | (r1.first & 7));
                break;
            }
            case ASM_SETG:
            { 
                assertExpr(node, r1.second == 1);
                if (needrex) { pbyte(rex); }
                pbyte(0x0F);
                pbyte(0x9F);
                pbyte(0xC0 | (0 << 3) | (r1.first & 7));
                break;
            }
            case ASM_SETGE:
            { 
                assertExpr(node, r1.second == 1);
                if (needrex) { pbyte(rex); }
                pbyte(0x0F);
                pbyte(0x9D);
                pbyte(0xC0 | (0 << 3) | (r1.first & 7));
                break;
            }
            case ASM_SETA:
            { 
                assertExpr(node, r1.second == 1);
                if (needrex) { pbyte(rex); }
                pbyte(0x0F);
                pbyte(0x97);
                pbyte(0xC0 | (0 << 3) | (r1.first & 7));
                break;
            }
            case ASM_SETAE:
            { 
                assertExpr(node, r1.second == 1);
                if (needrex) { pbyte(rex); }
                pbyte(0x0F);
                pbyte(0x93);
                pbyte(0xC0 | (0 << 3) | (r1.first & 7));
                break;
            }
            case ASM_SETB:
            { 
                assertExpr(node, r1.second == 1);
                if (needrex) { pbyte(rex); }
                pbyte(0x0F);
                pbyte(0x92);
                pbyte(0xC0 | (0 << 3) | (r1.first & 7));
                break;
            }
            case ASM_SETBE:
            { 
                assertExpr(node, r1.second == 1);
                if (needrex) { pbyte(rex); }
                pbyte(0x0F);
                pbyte(0x96);
                pbyte(0xC0 | (0 << 3) | (r1.first & 7));
                break;
            }
        }
    }

    
    void printRR(OperationBlock *node, asm_operation2 op, pair<int64_t, int64_t> r1, pair<int64_t, int64_t> r2)
    {
        // common data
        BYTE rex = 0x40;
        bool needrex = false;

        if (op == ASM_IMUL || op == ASM_TEST)
        {
            /* swap registers */
            swap(r1, r2);
        }

        if (r1.first & 8) { needrex = true; rex |= 0x01; }
        if (r2.first & 8) { needrex = true; rex |= 0x04; }
        if (r1.second == 1) { needrex |= (r1.first & 7) >= 4; }
        if (r2.second == 1) { needrex |= (r2.first & 7) >= 4; }
        if (r1.second == 8) { needrex = true; rex |= 0x08; }
        if (r2.second == 8) { needrex = true; rex |= 0x08; }

        // TODO: check all operations order
        
        switch (op)
        {
            // [prefixes] [REX] [88/89] [param]
            case ASM_MOV:
            {
                assertExpr(node, r1.second == r2.second);

                if (r1.second == 2) { pbyte(0x66); }
                
                if (needrex) { pbyte(rex); }

                pbyte((r1.second == 1 ? 0x88 : 0x89));
                pbyte(0xC0 | ((r2.first & 7) << 3) | (r1.first & 7));
                break;
            }
            // [prefixes] [REX] [0F] [B6/B7/BE/BF] [param]        
            case ASM_MOVZX:
            case ASM_MOVSX:
            {                
                // if this fallback isn't need, don't enable it
                // if (r1.second == r2.second) return printRR(ASM_MOV, r1, r2);
                
                assertExpr(node, r1.second > r2.second);
                
                rex = 0x40;
                needrex = false;
                
                if (r1.first & 8) { needrex = true; rex |= 0x04; }
                if (r2.first & 8) { needrex = true; rex |= 0x01; }
                if (r1.second == 1) { needrex |= (r1.first & 7) >= 4; }
                if (r2.second == 1) { needrex |= (r2.first & 7) >= 4; }
                if (r1.second == 8) { needrex = true; rex |= 0x08; }
                if (r2.second == 8) { needrex = true; rex |= 0x08; }

                switch (r2.second)
                {
                    case 1:
                        if (r1.second == 2) { pbyte(0x66); }
                            
                        if (needrex) { pbyte(rex); }

                        pbyte(0x0F);
                        pbyte((op == ASM_MOVZX) ? 0xB6 : 0xBE);
                        pbyte(0xC0 | ((r1.first & 7) << 3) | (r2.first & 7));
                        break;
                    case 2:
                        // dest is only 32/64
                        pbyte(0x66);

                        if (needrex) { pbyte(rex); }

                        pbyte(0x0F);
                        pbyte((op == ASM_MOVZX) ? 0xB7 : 0xBF);
                        pbyte(0xC0 | ((r1.first & 7) << 3) | (r2.first & 7));
                        break;
                    case 4:
                        // dest is only 8 bit
                        if (op == ASM_MOVZX)
                        {
                            // simple mov
                            printRR(node, ASM_MOV, {r1.first, 4}, r2);
                        }
                        else
                        {
                            pbyte(rex);
                            pbyte(0x63);
                            pbyte(0xC0 | ((r1.first & 7) << 3) | (r2.first & 7));
                        }
                        break;
                }
                break;
            }                
            // [prefixes] [REX] [88/89] [param]
            case ASM_XOR:
            {
                assertExpr(node, r1.second == r2.second);
                if (r1.second == 2) { pbyte(0x66); }
                if (needrex) { pbyte(rex); }
                pbyte((r1.second == 1 ? 0x30 : 0x31));
                pbyte(0xC0 | ((r2.first & 7) << 3) | (r1.first & 7));
                break;
            }
            case ASM_AND:
            {
                assertExpr(node, r1.second == r2.second);
                if (r1.second == 2) { pbyte(0x66); }
                if (needrex) { pbyte(rex); }
                pbyte((r1.second == 1 ? 0x20 : 0x21));
                pbyte(0xC0 | ((r2.first & 7) << 3) | (r1.first & 7));
                break;
            }
            case ASM_OR:
            {
                assertExpr(node, r1.second == r2.second);
                if (r1.second == 2) { pbyte(0x66); }
                if (needrex) { pbyte(rex); }
                pbyte((r1.second == 1 ? 0x08 : 0x09));
                pbyte(0xC0 | ((r2.first & 7) << 3) | (r1.first & 7));
                break;
            }
            case ASM_ADD:
            {
                assertExpr(node, r1.second == r2.second);
                if (r1.second == 2) { pbyte(0x66); }
                if (needrex) { pbyte(rex); }
                pbyte((r1.second == 1 ? 0x00 : 0x01));
                pbyte(0xC0 | ((r2.first & 7) << 3) | (r1.first & 7));
                break;
            }
            case ASM_SUB:
            {
                assertExpr(node, r1.second == r2.second);
                if (r1.second == 2) { pbyte(0x66); }
                if (needrex) { pbyte(rex); }
                pbyte((r1.second == 1 ? 0x28 : 0x29));
                pbyte(0xC0 | ((r2.first & 7) << 3) | (r1.first & 7));
                break;
            }
            case ASM_IMUL:
            {
                assertExpr(node, r1.second == r2.second);
                if (r1.second <= 2) { pbyte(0x66); } // multiplicate 8 byte as 16 byte
                if (needrex) { pbyte(rex); }
                pbyte(0x0F);
                pbyte(0xAF);
                pbyte(0xC0 | ((r2.first & 7) << 3) | (r1.first & 7));
                break;
            }
            case ASM_TEST:
            {
                // TODO: check test opcode args order
                assertExpr(node, r1.second == r2.second);
                if (r1.second == 2) { pbyte(0x66); }
                if (needrex) { pbyte(rex); }
                pbyte((r1.second == 1 ? 0x84 : 0x85));
                pbyte(0xC0 | ((r2.first & 7) << 3) | (r1.first & 7));
                break;
            }
            case ASM_CMP:
            {
                // printf("%lld %lld\n", r1.second, r2.second);
                assertExpr(node, r1.second == r2.second);
                if (r1.second == 2) { pbyte(0x66); }
                if (needrex) { pbyte(rex); }
                pbyte((r1.second == 1 ? 0x38 : 0x39));
                pbyte(0xC0 | ((r2.first & 7) << 3) | (r1.first & 7));
                break;
            }
        }
    }

    void printRM(OperationBlock *node, asm_operation3rm op, pair<int64_t, int64_t> r1, pair<int64_t, int64_t> r2, int64_t offset)
    {
        
        BYTE rex = 0x40;
        bool needrex = false;
        
        if (r1.first & 8) { needrex = true; rex |= 0x04; }
        if (r2.first & 8) { needrex = true; rex |= 0x01; }
        if (r1.second == 8) { needrex = true; rex |= 0x08; }

        switch (op)
        {
            case ASM_MOV_RM:
            {   
                assertExpr(node, r2.second == 8);

                // add 16 bit prefix
                if (r1.second == 2)  { pbyte(0x66); }

                // add rex
                if (needrex)  { pbyte(rex); }

                // opcode
                pbyte((r1.second == 1 ? 0x8A : 0x8B));

                // modrm
                if (offset == 0 && (r2.first & 7) != 5) // if not rbp/r13
                {
                    pbyte(0x00 | ((r1.first & 7) << 3) | (r2.first & 7));
                } 
                else if (offset >= INT8_MIN && offset <= INT8_MAX) 
                {
                    pbyte(0x40 | ((r1.first & 7) << 3) | (r2.first & 7));
                } 
                else 
                {
                    pbyte(0x80 | ((r1.first & 7) << 3) | (r2.first & 7));
                }
                
                if ((r2.first & 7) == 4)  // if rsp/r12
                {
                    // forced to use SIB
                    pbyte((0x00 << 6) | (0x04 << 3) | (r2.first & 7));
                } 

                // offset
                if (offset || (r2.first & 7) == 5) // if rbp/r13 - write offset even if it is 0
                {
                    if (offset >= INT8_MIN && offset <= INT8_MAX) 
                    {
                        pbyte(offset);
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

    void printRC(OperationBlock *node, asm_operation2rc op, pair<int64_t, int64_t> r1, int64_t value)
    {
        (void)node;
                    
        switch (op)
        {
            case ASM_MOV_RC:
                if (r1.second == 8 && value == (uint32_t)value) { r1.second = 4; }
                break;
            default:
                break;
        }
        
        BYTE rex = 0x40;
        bool needrex = false;
    
        if (r1.first & 8)
        {
            rex |= 0x01;
            needrex = true;
        }
        if (r1.second == 1 && r1.first >= 4)
        {
            needrex = true;
        }
        if (r1.second == 8)
        {
            rex |= 0x08;
            needrex = true;
        }
        
        
        switch (op)
        {
            case ASM_MOV_RC:
            {
                if (r1.second == 2) { pbyte(0x66); }
                
                if (needrex) { pbyte(rex); }
                
                pbyte((r1.second == 1 ? 0xB0 : 0xB8) | (r1.first & 7));

                memcpy(assemblyEnd, &value, r1.second);
                assemblyEnd += r1.second;
                
                break;
            }
            case ASM_ADD_RC:
            {
                if (r1.second == 2) { pbyte(0x66); }
                
                if (needrex) { pbyte(rex); }
                
                if (value >= INT8_MIN || value <= INT8_MAX)
                {
                    pbyte((r1.second == 1 ? 0x80 : 0x83));
                    pbyte(0xC0 | (r1.first & 7));
                    pbyte(value);
                }
                else if (value >= INT32_MIN || value <= INT32_MAX)
                {
                    pbyte((r1.second == 1 ? 0x80 : 0x81));
                    pbyte(0xC0 | (r1.first & 7));
                    memcpy(assemblyEnd, &value, min(4LL, r1.second));
                    assemblyEnd += min(4LL, r1.second);
                }
                else
                {
                    logError(ir->filename, "", 0, 0, "Can't use ADD_RC with 64bit constant");
                    return;
                }
                
                break;
            }
        }
    }

    void printMR(OperationBlock *node, asm_operation3mr op, pair<int64_t, int64_t> r1, pair<int64_t, int64_t> r2, int64_t offset)
    {
        
        BYTE rex = 0x40;
        bool needrex = false;
        
        if (r1.first & 8) { needrex = true; rex |= 0x01; }
        if (r2.first & 8) { needrex = true; rex |= 0x04; }
        if (r2.second == 8) { needrex = true; rex |= 0x08; }
        if (r1.second == 1 && r1.first >= 4) { needrex = true; }
        if (r2.second == 1 && r2.first >= 4) { needrex = true; }
        switch (op)
        {
            case ASM_MOV_MR:
            {   
                assertExpr(node, r1.second == 8);

                if (r2.second == 2)  { pbyte(0x66); }

                // rex
                if (needrex)  { pbyte(rex); }

                // opcode
                pbyte((r2.second == 1 ? 0x88 : 0x89));

                // modrm
                if (offset == 0 && (r1.first & 7) != 5) // if not rbp/r13
                {
                    pbyte(0x00 | ((r2.first & 7) << 3) | (r1.first & 7));
                } 
                else if (offset >= INT8_MIN && offset <= INT8_MAX) 
                {
                    pbyte(0x40 | ((r2.first & 7) << 3) | (r1.first & 7));
                } 
                else 
                {
                    pbyte(0x80 | ((r2.first & 7) << 3) | (r1.first & 7));
                }
                
                if ((r1.first & 7) == 4)  // if rsp/r12
                {
                    // forced to use SIB
                    pbyte((0x00 << 6) | (0x04 << 3) | (r1.first & 7));
                } 

                // offset
                if (offset || (r1.first & 7) == 5) // if rbp/r13 - write offset even if zero
                {
                    if (offset >= INT8_MIN && offset <= INT8_MAX) 
                    {
                        pbyte(offset);
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

    void printRRC(OperationBlock *node, asm_operation3rrc op, pair<int64_t, int64_t> r1, pair<int64_t, int64_t> r2, int64_t constant)
    {
        bool needrex = false;
        int64_t rex = 0x40;

        if (r1.first & 8) { needrex = true; rex |= 0x04; }
        if (r2.first & 8) { needrex = true; rex |= 0x01; }
        if (r2.second == 8) { needrex = true; rex |= 0x08; }

        if (r1.second == 1 && r1.first >= 4) { needrex = true; }
        if (r2.second == 1 && r2.first >= 4) { needrex = true; }
        
        switch (op)
        {
            case ASM_IMUL_RRC:
                assertExpr(node, r1.second == r2.second);
                if (r1.second == 1)
                {
                    logError(ir->filename, "", 0, 0, "IMUL can't take byte variables");
                    return;
                }

                if (r2.second == 2) pbyte(0x66);
                
                if (needrex) pbyte(rex);

                if (constant >= INT8_MIN && constant <= INT8_MAX)
                {
                    pbyte(0x6B);
                    pbyte(0xC0 | ((r1.first & 7) << 3) | (r2.first & 7));
                    pbyte(constant);
                }               
                else if (constant >= INT32_MIN && constant <= INT32_MAX)
                {
                    pbyte(0x69);
                    pbyte(0xC0 | ((r1.first & 7) << 3) | (r2.first & 7));
                    memcpy(assemblyEnd, &constant, 8);
                    assemblyEnd += 8;
                } 
                else 
                {
                    logError(ir->filename, "", 0, 0, "IMUL can't take 64bit constants");
                    return;
                }
                break;
        }
    }
    
    void printRRR(OperationBlock *node, asm_operation3 op, pair<int64_t, int64_t> r1, pair<int64_t, int64_t> r2, pair<int64_t, int64_t> r3)
    {

        assertExpr(node, r1.second == r2.second && r2.second == r3.second);
        assertExpr(node, r1.second == 4 || r1.second == 8);

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

                pbyte(0xC4);
                
                BYTE r_bit = !(r1.first & 8);
                BYTE x_bit = 1;
                BYTE b_bit = !(r2.first & 8);
                BYTE map_select = 0x02; // can't use C5 vex
                
                pbyte((r_bit << 7) | (x_bit << 6) | (b_bit << 5) | map_select);
                
                BYTE W = (r1.second == 8) ? 1 : 0;
                BYTE vvvv = (~r3.first) << 3;
                BYTE L = 0;
                
                pbyte((W << 7) | vvvv | (L << 2) | pp);

                // modrm
                pbyte(0xF7);
                
                BYTE modrm = 0xC0 | ((r1.first & 7) << 3) | (r2.first & 7);
                pbyte(modrm);
                break;
            }
        }
    }

    // return {size, encoding variant}
    pair<int64_t, int64_t> JMPsize(OperationBlock *node, asm_operation_jmp op, int64_t offset, int64_t encoding_variant)
    {
        (void)node;
        
        if (offset >= INT8_MIN + 6 && offset <= INT8_MAX && encoding_variant <= 0)
        {   
            return {1 + 1, 0}; // opcode + data
        }
        else if (encoding_variant <= 1)
        {
            switch (op)
            {
                case ASM_JMP: return {1 + 4, 1}; // opcode + data
                case ASM_JZ:
                case ASM_JNZ:
                case ASM_JS:
                case ASM_JNS:
                case ASM_JNAE:
                case ASM_JB:
                case ASM_JC:
                case ASM_NC:
                case ASM_NB:
                case ASM_JAE:
                case ASM_JO:
                case ASM_JNO: return {2 + 4, 1}; // opcode + data
            }
        }
        else
        {
            logError(ir->filename, "", 0, 0, "Wrong encoding_variant: %lld", encoding_variant);
            return {0, 0};
        }
    }

    int64_t printJMP(OperationBlock *node, asm_operation_jmp op, int64_t offset, int64_t encoding_variant)
    {
        (void)node;
        
        auto [sz, var] = JMPsize(node, op, offset, encoding_variant);
        offset -= sz;
        // 6 is max jmp size
        if (var == 0)
        {   
            switch (op)
            {
                case ASM_JMP: pbyte(0xEB); break;
                case ASM_JZ: pbyte(0x74); break;
                case ASM_JNZ: pbyte(0x75); break;
                case ASM_JS: pbyte(0x78); break;
                case ASM_JNS: pbyte(0x79); break;
                case ASM_JNAE:
                case ASM_JB:
                case ASM_JC: pbyte(0x72); break;
                case ASM_NC:
                case ASM_NB:
                case ASM_JAE: pbyte(0x73); break;
                case ASM_JO: pbyte(0x70); break;
                case ASM_JNO: pbyte(0x71); break;
            }   
            
            pbyte(offset);
            return 0;
        }
        else if (var == 1)
        {
            switch (op)
            {
                case ASM_JMP: pbyte(0xE9); break;
                case ASM_JZ: pbyte(0x0F, 0x84); break;
                case ASM_JNZ: pbyte(0x0F, 0x85); break;
                case ASM_JS: pbyte(0x0F, 0x88); break;
                case ASM_JNS: pbyte(0x0F, 0x89); break;
                case ASM_JNAE:
                case ASM_JB:
                case ASM_JC: pbyte(0x0F, 0x82); break;
                case ASM_NC:
                case ASM_NB:
                case ASM_JAE: pbyte(0x0F, 0x83); break;
                case ASM_JO: pbyte(0x0F, 0x80); break;
                case ASM_JNO: pbyte(0x0F, 0x81); break;
            }
            
            memcpy(assemblyEnd, &offset, 4);
            assemblyEnd += 4;
            return 1;
        }
        else
        {
            logError(ir->filename, "", 0, 0, "Wrong encoding_variant: %lld", var);
            return -1;
        }
    }

    BYTE *printCALL(OperationBlock *node, int64_t address)
    {
        (void)node;
        
        pbyte(0x48, 0xB8); // mov rax im64
        BYTE *res = assemblyEnd;
        memcpy(assemblyEnd, &address, 8);
        assemblyEnd += 8;
        pbyte(0xFF, 0xD0); // call rax
        return res;
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
    map<int64_t, int64_t> resultWorkerPositions;
    
    void Build(BuildResult *input, const char *resultFileName) override 
    {
        ir = input;
        printf("Building into %s\n", resultFileName);

        /* init build context */
        nextLabelId = 0;
        addressTable.clear();

        /* initializate string */
        assemblyAlloc = 0;
        assemblyCode = assemblyEnd = (BYTE *)malloc(1024 * 1024);

        /* build each worker */
        for (auto &[fn, id] : ir->workers) { idToWorker[id] = fn; }
        for (auto &[fn, id] : ir->workers) { BuildFn(fn, id); }

        // add terminating zero
        printf("Code addressTable.\n");
        if (!assemblyEnd)
        {
            return;
        }

        for (BYTE *x = assemblyCode; x < assemblyEnd; ++x)
        {
            printf(" %02X", *x);
        }
        printf("\n");

        ExportToFile(resultFileName);
    }

private:
    WorkerDeclarationContext *current;
    RegisterAnalizator *analyzer;
    // registers table
    map<int64_t, int64_t> regTable;
    map<int64_t, int64_t> memTable;
    int64_t usedMemory;
    map<OperationBlock *, BYTE *> addressTable;
    map<OperationBlock *, int64_t> orderTable;
    vector<jmpInstruction> JumpInstructions;

    struct api_call_entry
    {
        int64_t position;
        int64_t order;
    };

    // header key, value
    #define HEADER_ENTRY_PUSH_OBJECT 0
    #define HEADER_ENTRY_PUSH_PIPE 8
    #define HEADER_ENTRY_QUERY_OBJECT 1
    #define HEADER_ENTRY_QUERY_PIPE 9
    #define HEADER_ENTRY_NEW_OBJECT 2
    #define HEADER_ENTRY_CALL_OBJECT 3
    map<BYTE, vector<api_call_entry>> runtimeApiHeader;
    
    #define HEADER_ENTRY_DLL_IMPORT 4
    // worker id -> vector of input sizes + output size
    struct dll_import_entry
    {
        int64_t output;
        vector<int64_t> inputs;
        string library;
        string entry;
    };
    map<int64_t, dll_import_entry> dllImportHeader;

    vector<pair<OperationBlock *, OperationBlock *>> toBuild;

    bool isSigned(int64_t name)
    {
        return SCALAR_TYPE(varType(name)->_scalar.kind) == SCALAR_I;
    }

    bool isApiScalar(int64_t name)
    {
        return (varType(name)->type != TYPE_RECORD && varType(name)->type != TYPE_UNION);
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

    int64_t GetWorkerInputTableSize(int64_t id)
    {
        int64_t res = 0;
        
        for (auto &[name, type] : array{views::all(idToWorker[id]->inputs), 
                                        views::all(idToWorker[id]->outputs)} | views::join)
        {
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

    /*
        registers: 
            rbp - pointer on locals
            rdi - pointer on inputs table + used in api calls // TODO: optimize?
            rsi - used in api calls
            rcx - used in api calls
            rax - used for division / api calls
            rdx - used for division / api calls

        [rdi is used in api calls, becouse of assumptions of all api calls be after LOAD_INPUT]
    */


    // registers configuraion
    static constexpr int64_t registersCount = 9;
    // all other registers
    const int64_t registers[registersCount] = {0b0011, 0b1000, 0b1001, 0b1010, 0b1011, 0b1100, 0b1101, 0b1110, 0b1111};

    const pair<int64_t, int64_t> Register(int64_t var)
    {
        return {registers[regTable[var]], varSize(var)};
    }

    const pair<int64_t, int64_t> Register(int64_t var, int64_t size)
    {
        return {registers[regTable[var]], size};
    }

    void BuildFn(WorkerDeclarationContext *wk, int64_t workerId)
    {
        if (wk->content == NULL)
        {
            // if this function is extern, create it's header
            if (wk->attributes.contains("dllimport") && holds_alternative<string>(wk->attributes["dllimport"]))
            {
                string &lib = get<string>(wk->attributes["dllimport"]);
                string entry = (wk->attributes.contains("dllimport.entry") && holds_alternative<string>(wk->attributes["dllimport.entry"])
                               ? get<string>(wk->attributes["dllimport.entry"]) : wk->name);
                if (wk->outputs.size() > 1)
                {
                    logError(ir->filename, ir->code, wk->code_start, wk->code_end, "DLLIMPORT function have more than 1 return argument");
                    return;
                }
                if (!wk->outputs.empty() && wk->outputs[0].second->type != TYPE_PROMISE)
                {
                    logError(ir->filename, ir->code, wk->code_start, wk->code_end, "DLLIMPORT function's return argument isn't promise");
                    return;
                }
                int64_t out_size = (wk->outputs.empty() ? -1 : wk->outputs[0].second->_vector.base->size);
                // generate parameter sizes
                vector<int64_t> sizes;
                for (auto &[name, type] : wk->inputs)
                {
                    sizes.push_back(type->size);
                }
                // for now, there is no extra marshalling
                dllImportHeader[workerId] = {out_size, sizes, lib, entry};
            }
            return;
        }
        
        current = wk;

        // update call instructions
        ExpandCallInstructions(wk);

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
            for (auto &[k, v] : memTable)
            {
                v += 8;
            }
        }

        // print memory table
        for (auto &[k, v] : memTable)
        {
            printf("Var %lld have offset %lld\n", k, v);
        }
        printf("Total size of variables: %lld\n", usedMemory);

        dumpIR(wk);

        // analyze code
        analyzer = new RegisterAnalizator(wk);

        // get used labels
        JumpInstructions.clear();
        
        // generate code

        resultWorkerPositions[workerId] = assemblyEnd - assemblyCode;
        
        print("worker_%lld:\n", workerId);
        if (wk->content == NULL)
        {
            printf("For now, x64-win doesn't support dynamic linking\n");
        }
        else
        {
            addressTable.clear();
            orderTable.clear();

            toBuild.push_back({wk->content->entry, NULL});
            while (!toBuild.empty())
            {
                BuildOperation();
            }
        }

        // join code using JumpInstructions

        InsertJumpInstructions();

        delete analyzer;
    }

    void ExpandCallInstructions(WorkerDeclarationContext *wk)
    {
        for (int64_t i = 0; i < (int64_t)wk->content->code.size(); ++i)
        {
            auto op = wk->content->code[i];
            if (op->type == OP_CALL)
            {
                WorkerDeclarationContext *fn = idToWorker[op->data[0]];
                // insert before it input table generation
                int64_t id = 0, offset = 0, inputTableSize = GetWorkerInputTableSize(op->data[0]);
                for (auto &[name, type] : array{views::all(fn->inputs), 
                                                views::all(fn->outputs)} | views::join)
                {
                    int64_t tmp = op->data[id + 1];
                    if (id < (int64_t)fn->inputs.size() && wk->content->variables[op->data[id + 1]] != fn->inputs[id].second)
                    {
                        tmp = newTemp(wk, fn->inputs[id].second);
                        auto *castOp = new OperationBlock(OP_CAST, {tmp, op->data[id + 1]});
                        connectBeforeOp(wk, op, castOp);
                    }
                    
                    auto *newOp = new OperationBlock(OP_STORE_INPUT, {tmp, offset - inputTableSize});
                    connectBeforeOp(wk, op, newOp);
                    
                    if (tmp != op->data[id + 1])
                    {
                        auto *freeOp = new OperationBlock(OP_FREE_TEMP, {tmp});
                        connectOp(wk, op, freeOp);
                    }
                    
                    offset += type->size;
                    id++;
                }
                
                // now it have only 1st data
                op->data.resize(1);
            }
        }
    }

    void InsertMove(OperationBlock *node, pair<int64_t, int64_t> dest, pair<int64_t, int64_t> from, bool isSigned)
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

    void InsertMove(OperationBlock *node, int64_t dest, int64_t from)
    {
        InsertMove(node, Register(dest), Register(from), isSigned(dest));
    }

    void InsertInteger(OperationBlock *node, pair<int64_t, int64_t> dest, int64_t value)
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

    void ExternTo64Bit(OperationBlock *node, pair<int64_t, int64_t> reg, bool is_signed)
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

    int64_t GetClassSize(TypeContext *type)
    {
        int64_t sum = 0;
        for (auto &i : type->_struct.fields)
        {
            sum += i->size;
        }
        return sum;
    }

    void BuildOperation()
    {
        OperationBlock *op = toBuild.back().first;
        OperationBlock *prevOp = toBuild.back().second;
        toBuild.pop_back();
        
        // return from function
        if (op == NULL)
        {
            pbyte(0xC3);
            return;
        }

        int64_t currentOrder = orderTable.size();
        
        // if already this block is compiled - jump to it
        // TODO: here can take some place of "assembly inlining"
        if (!addressTable.insert({op, assemblyEnd}).second) 
        { 
            JumpInstructions.push_back({ASM_JMP, assemblyEnd, currentOrder, op, prevOp});
            return;
        }

        orderTable[op] = currentOrder;

        // if instruction have many next:
        #define ABEL_BINOP(T) \
            if (regTable[op->data[0]] == regTable[op->data[2]]) \
            { \
                printRR(op, T, Register(op->data[2]), Register(op->data[1])); \
            } \
            else \
            { \
                InsertMove(op, op->data[0], op->data[1]); \
                printRR(op, T, Register(op->data[0]), Register(op->data[2])); \
            }
        #define CMPOP(A, B) { \
            printf("%lld %lld %lld [%p %p %p]\n", op->data[0], op->data[1], op->data[2], varType(op->data[0]), varType(op->data[1]), varType(op->data[2])); \
            printRR(op, ASM_CMP, Register(op->data[1]), Register(op->data[2])); \
            printRC(op, ASM_MOV_RC, Register(op->data[0]), 0); \
            auto reg = Register(op->data[0]); \
            reg.second = 1; \
            if (isSigned(op->data[0])) \
                printR(op, A, reg); \
            else \
                printR(op, B, reg); \
            printR(op, ASM_NEG, Register(op->data[0])); \
        }
        
        switch (op->type)
        {
            // impossible
            case OP_JMP: break;    
            // nothing to do
            case OP_FREE_TEMP: break;
            
            case OP_JZ:
            {
                printRR(op, ASM_TEST, Register(op->data[0]), Register(op->data[0]));
                JumpInstructions.push_back({ASM_JZ, assemblyEnd, currentOrder, op->next[1], op});
                break;
            }   
            case OP_JNZ: 
            {
                printRR(op, ASM_TEST, Register(op->data[0]), Register(op->data[0]));
                JumpInstructions.push_back({ASM_JNZ, assemblyEnd, currentOrder, op->next[1], op});
                break;
            }
            case OP_LOAD:
                // mov $0, XX PTR [rbp + $1]
                printRM(op, ASM_MOV_RM, Register(op->data[0]), {5, 8}, memTable[op->data[1]]);
                break;
            case OP_STORE:
                // mov XX PTR [rbp + $1], $0
                printMR(op, ASM_MOV_MR, {5, 8}, Register(op->data[0]), memTable[op->data[1]]);
                break;
        
            case OP_LOAD_INPUT: 
                // mov $0, XX PTR [rdi + $1]
                printRM(op, ASM_MOV_RM, Register(op->data[1]), {7, 8}, GetInputOffset(op->data[0]));
                break;
            case OP_LOAD_OUTPUT: 
                // mov $0, XX PTR [rdi + $1]
                printRM(op, ASM_MOV_RM, Register(op->data[1]), {7, 8}, GetOutputOffset(op->data[0]));
                break;
            
            case OP_STORE_INPUT:
            {
                printMR(op, ASM_MOV_MR, {5, 8}, Register(op->data[0]), op->data[1]);
                break;
            }
            
            case OP_CALL:
            {
                // call table was generated, use it
                int64_t callTableSize = GetWorkerInputTableSize(op->data[0]);
                // rdx=worker id
                // rsi=call table
                // rdi="on" parameter
                InsertInteger(op, {2, 8}, op->data[0]);
                InsertMove(op, {6, 8}, {5, 8}, false);
                if (op->attributes.contains("on"))
                {
                    if (op->attributes["on"] == string("local"))
                    {
                        InsertInteger(op, {7, 8}, 1);
                    }
                    else if (op->attributes["on"] == string("remote"))
                    {
                        InsertInteger(op, {7, 8}, 2);
                    }
                    else if (holds_alternative<int64_t>(op->attributes["on"]))
                    {
                        // this is temporary ID that holds global compueter name
                        InsertMove(op, {7, 8}, Register(get<int64_t>(op->attributes["on"])), false);
                    }
                    else
                    {
                        logError(ir->filename, "", op->code_start, op->code_end, "Wrong \"on\" call attribute value: %s", get<string>(op->attributes["on"]).c_str());
                    }
                }
                else { InsertInteger(op, {7, 8}, 0); } // default run attribute
                printRC(op, ASM_ADD_RC, {6, 8}, -callTableSize);
                runtimeApiHeader[HEADER_ENTRY_CALL_OBJECT].push_back({printCALL(op, 0x0) - assemblyCode, currentOrder});
                break;
            }
                
            case OP_CAST: 
                InsertMove(op, op->data[0], op->data[1]);
                break;
                
            case OP_MOV: 
                InsertMove(op, op->data[0], op->data[1]);
                break;

            case OP_NEW_INT: 
                InsertInteger(op, Register(op->data[0], 8), op->data[1]);
                break;
                
            case OP_NEW_FLOAT:
                print("\tOP_NEW_FLOAT [not supported]\n"); 
                break;
                
            case OP_NEW_STRING:
            {
                // rdi=OBJECT_DEFINED_ARRAY=5
                // rsi=defined object ID
                // rdx=size of element
                InsertInteger(op, {7, 8}, 0x05);
                InsertInteger(op, {6, 8}, op->data[1]);
                InsertInteger(op, {2, 8}, varType(op->data[0])->_vector.base->size);
                runtimeApiHeader[HEADER_ENTRY_NEW_OBJECT].push_back({printCALL(op, 0x0) - assemblyCode, currentOrder});
                InsertMove(op, Register(op->data[0]), {7, 8}, false);
                break;
            }
            case OP_NEW_ARRAY:
            {
                // rdi=OBJECT_ARRAY=3
                // rsi=total size
                // rdx=size of element
                InsertInteger(op, {7, 8}, 0x03);
                ExternTo64Bit(op, Register(op->data[1]), isSigned(op->data[1]));
                printRRC(op, ASM_IMUL_RRC, {6, 8}, Register(op->data[1], 8), varType(op->data[0])->_vector.base->size);
                InsertInteger(op, {2, 8}, varType(op->data[0])->_vector.base->size);
                runtimeApiHeader[HEADER_ENTRY_NEW_OBJECT].push_back({printCALL(op, 0x0) - assemblyCode, currentOrder});
                InsertMove(op, Register(op->data[0]), {7, 8}, false);
                break;
            }
            case OP_NEW_PROMISE:
            {
                // rdi=OBJECT_PROMISE=2
                // rsi=total size
                // rdx=size of element = total size
                InsertInteger(op, {7, 8}, 0x02);
                InsertInteger(op, {6, 8}, varType(op->data[0])->_vector.base->size);
                InsertMove(op, {2, 8}, {6, 8}, false);
                runtimeApiHeader[HEADER_ENTRY_NEW_OBJECT].push_back({printCALL(op, 0x0) - assemblyCode, currentOrder});
                InsertMove(op, Register(op->data[0]), {7, 8}, false);
                break;
            }
            case OP_NEW_CLASS:
            {
                // rdi=OBJECT_OBJECT=4
                // rsi=total size
                // rdx=size of element = total size
                int64_t cls_size = GetClassSize(varType(op->data[0]));
                InsertInteger(op, {7, 8}, 0x04);
                InsertInteger(op, {6, 8}, cls_size);
                InsertMove(op, {2, 8}, {6, 8}, false);
                runtimeApiHeader[HEADER_ENTRY_NEW_OBJECT].push_back({printCALL(op, 0x0) - assemblyCode, currentOrder});
                InsertMove(op, Register(op->data[0]), {7, 8}, false);
                break;
            }
            case OP_NEW_PIPE:
            {
                // rdi=OBJECT_PIPE=1
                // rsi=total size
                // rdx=size of element = total size
                InsertInteger(op, {7, 8}, 0x01);
                InsertInteger(op, {6, 8}, varType(op->data[0])->_vector.base->size);
                InsertMove(op, {2, 8}, {6, 8}, false);
                runtimeApiHeader[HEADER_ENTRY_NEW_OBJECT].push_back({printCALL(op, 0x0) - assemblyCode, currentOrder});
                InsertMove(op, Register(op->data[0]), {7, 8}, false);
                break;
            }
            
            case OP_PUSH_VAR:
                // TODO: what if data[3] is not scalar?
                assert(op->data[1] != 0 || (TypeContext *)op->data[2] != varType(op->data[0]));
                // mov XX PTR [rbp + $0 + $1], $3
                printMR(op, ASM_MOV_MR, {5, 8}, Register(op->data[3]), memTable[op->data[0]] + op->data[1]);
                // (TypeContext *)op->data[2] - type [unused for now]
                break;
                
            case OP_PUSH_ARRAY:
            {
                // TODO: remove usage of source as size provider
                // rcx=size rdx=offset rdi=object rsi=value
                if (isApiScalar(op->data[4]))
                {
                    InsertInteger(op, {1, 8}, -varSize(op->data[4]));
                    ExternTo64Bit(op, Register(op->data[1]), isSigned(op->data[1]));
                    printRRC(op, ASM_IMUL_RRC, {2, 8}, Register(op->data[1], 8), varType(op->data[0])->_vector.base->size);
                    if (op->data[2] != 0) { printRC(op, ASM_ADD_RC, {2, 8}, op->data[2]); }
                    InsertMove(op, {7, 8}, Register(op->data[0]), false);
                    InsertMove(op, {6, 8}, Register(op->data[4]), false);
                    runtimeApiHeader[HEADER_ENTRY_PUSH_OBJECT].push_back({printCALL(op, 0x0) - assemblyCode, currentOrder});
                }
                else
                {
                    InsertInteger(op, {1, 8}, varSize(op->data[4]));
                    ExternTo64Bit(op, Register(op->data[1]), isSigned(op->data[1]));
                    printRRC(op, ASM_IMUL_RRC, {2, 8}, Register(op->data[1], 8), varType(op->data[0])->_vector.base->size);
                    if (op->data[2] != 0) { printRC(op, ASM_ADD_RC, {2, 8}, op->data[2]); }
                    InsertMove(op, {7, 8}, Register(op->data[0]), false);
                    InsertInteger(op, {6, 8}, memTable[op->data[4]]);
                    runtimeApiHeader[HEADER_ENTRY_PUSH_OBJECT].push_back({printCALL(op, 0x0) - assemblyCode, currentOrder});
                }
                break;
            }
            
            case OP_PUSH_PROMISE:
            {
                // TODO: remove usage of source as size provider
                // rcx=size rdx=offset rdi=object rsi=value
                if (isApiScalar(op->data[1]))
                {
                    InsertInteger(op, {1, 8}, -varSize(op->data[1]));
                    InsertInteger(op, {2, 8}, 0);
                    InsertMove(op, {7, 8}, Register(op->data[0]), false);
                    InsertMove(op, {6, 8}, Register(op->data[1]), false);
                    runtimeApiHeader[HEADER_ENTRY_PUSH_OBJECT].push_back({printCALL(op, 0x0) - assemblyCode, currentOrder});
                }
                else
                {
                    InsertInteger(op, {1, 8}, varSize(op->data[1]));
                    InsertInteger(op, {2, 8}, 0);
                    InsertMove(op, {7, 8}, Register(op->data[0]), false);
                    InsertInteger(op, {6, 8}, memTable[op->data[1]]);
                    runtimeApiHeader[HEADER_ENTRY_PUSH_OBJECT].push_back({printCALL(op, 0x0) - assemblyCode, currentOrder});
                }
                break;
            }
            
            case OP_PUSH_PIPE:
            {
                // TODO: remove usage of source as size provider
                // rcx=size rdx=offset rdi=object rsi=value
                if (isApiScalar(op->data[1]))
                {
                    InsertInteger(op, {1, 8}, -varSize(op->data[1]));
                    InsertInteger(op, {2, 8}, 0);
                    InsertMove(op, {7, 8}, Register(op->data[0]), false);
                    InsertMove(op, {6, 8}, Register(op->data[1]), false);
                    runtimeApiHeader[HEADER_ENTRY_PUSH_PIPE].push_back({printCALL(op, 0x0) - assemblyCode, currentOrder});
                }
                else
                {
                    InsertInteger(op, {1, 8}, varSize(op->data[1]));
                    InsertInteger(op, {2, 8}, 0);
                    InsertMove(op, {7, 8}, Register(op->data[0]), false);
                    InsertInteger(op, {6, 8}, memTable[op->data[1]]);
                    runtimeApiHeader[HEADER_ENTRY_PUSH_PIPE].push_back({printCALL(op, 0x0) - assemblyCode, currentOrder});
                }
                break;
            }
            
            case OP_PUSH_CLASS:
            {
                // TODO: remove usage of source as size provider
                // rcx=size rdx=offset rdi=object rsi=value
                if (isApiScalar(op->data[3]))
                {
                    InsertInteger(op, {1, 8}, -varSize(op->data[3]));
                    InsertInteger(op, {2, 8}, op->data[1]);
                    InsertMove(op, {7, 8}, Register(op->data[0]), false);
                    InsertMove(op, {6, 8}, Register(op->data[3]), false);
                    runtimeApiHeader[HEADER_ENTRY_PUSH_OBJECT].push_back({printCALL(op, 0x0) - assemblyCode, currentOrder});
                }
                else
                {
                    InsertInteger(op, {1, 8}, varSize(op->data[3]));
                    InsertInteger(op, {2, 8}, op->data[1]);
                    InsertMove(op, {7, 8}, Register(op->data[0]), false);
                    InsertInteger(op, {6, 8}, memTable[op->data[4]]);
                    runtimeApiHeader[HEADER_ENTRY_PUSH_OBJECT].push_back({printCALL(op, 0x0) - assemblyCode, currentOrder});
                }
                break;
            }
                
            case OP_QUERY_VAR: 
            {
                // TODO: what if data[3] is not scalar?
                assert(op->data[2] != 0 || (TypeContext *)op->data[3] != varType(op->data[0]));
                // mov $0, XX PTR [rbp + $1 + $2]
                printRM(op, ASM_MOV_RM, Register(op->data[0]), {5, 8}, memTable[op->data[1]] + op->data[2]);
                // (TypeContext *)op->data[3] - type [unused for now]
                break;
            }
                
            case OP_QUERY_INDEX:
            {
                // TODO: remove usage of destination as size provider
                // rcx=size rdx=offset rdi=value rsi=object
                if (isApiScalar(op->data[0]))
                {
                    InsertInteger(op, {1, 8}, -varSize(op->data[0]));
                    ExternTo64Bit(op, Register(op->data[4]), isSigned(op->data[4]));
                    printRRC(op, ASM_IMUL_RRC, {2, 8}, Register(op->data[4], 8), varType(op->data[1])->_vector.base->size);
                    if (op->data[2] != 0) { printRC(op, ASM_ADD_RC, {2, 8}, op->data[2]); }
                    InsertMove(op, {6, 8}, Register(op->data[1]), false);
                    runtimeApiHeader[HEADER_ENTRY_QUERY_OBJECT].push_back({printCALL(op, 0x0) - assemblyCode, currentOrder});
                    InsertMove(op, Register(op->data[0]), {7, 8}, false);
                }
                else
                {
                    InsertInteger(op, {1, 8}, varSize(op->data[0]));
                    ExternTo64Bit(op, Register(op->data[4]), isSigned(op->data[4]));
                    printRRC(op, ASM_IMUL_RRC, {2, 8}, Register(op->data[4], 8), varType(op->data[1])->_vector.base->size);
                    if (op->data[2] != 0) { printRC(op, ASM_ADD_RC, {2, 8}, op->data[2]); }
                    InsertInteger(op, {7, 8}, memTable[op->data[0]]);
                    InsertMove(op, {6, 8}, Register(op->data[4]), false);
                    runtimeApiHeader[HEADER_ENTRY_QUERY_OBJECT].push_back({printCALL(op, 0x0) - assemblyCode, currentOrder});
                }
                break;
            }
            
            case OP_QUERY_ARRAY:
            {
                // rcx=size rdx=offset rdi=value rsi=object
                // TODO: remove usage of destination as size provider
                if (isApiScalar(op->data[0]))
                {
                    InsertInteger(op, {1, 8}, -varSize(op->data[0]));
                    InsertInteger(op, {2, 8}, -16);
                    InsertMove(op, {6, 8}, Register(op->data[1]), false);
                    runtimeApiHeader[HEADER_ENTRY_QUERY_OBJECT].push_back({printCALL(op, 0x0) - assemblyCode, currentOrder});
                    InsertMove(op, Register(op->data[0]), {7, 8}, false);
                }
                else
                {
                    InsertInteger(op, {1, 8}, varSize(op->data[0]));
                    InsertInteger(op, {2, 8}, -16);
                    InsertInteger(op, {7, 8}, memTable[op->data[0]]);
                    InsertMove(op, {6, 8}, Register(op->data[1]), false);
                    runtimeApiHeader[HEADER_ENTRY_QUERY_OBJECT].push_back({printCALL(op, 0x0) - assemblyCode, currentOrder});
                }
                break;
            }
            
            case OP_QUERY_PROMISE:
            {
                // TODO: remove usage of destination as size provider
                // rcx=size rdx=offset rdi=value rsi=object
                if (isApiScalar(op->data[0]))
                {
                    InsertInteger(op, {1, 8}, -varSize(op->data[0]));
                    InsertInteger(op, {2, 8}, 0);
                    InsertMove(op, {6, 8}, Register(op->data[1]), false);
                    runtimeApiHeader[HEADER_ENTRY_QUERY_OBJECT].push_back({printCALL(op, 0x0) - assemblyCode, currentOrder});
                    InsertMove(op, Register(op->data[0]), {7, 8}, false);
                }
                else
                {
                    InsertInteger(op, {1, 8}, varSize(op->data[0]));
                    InsertInteger(op, {2, 8}, 0);
                    InsertInteger(op, {7, 8}, memTable[op->data[0]]);
                    InsertMove(op, {6, 8}, Register(op->data[1]), false);
                    runtimeApiHeader[HEADER_ENTRY_QUERY_OBJECT].push_back({printCALL(op, 0x0) - assemblyCode, currentOrder});
                }
                break;
            }
            
            case OP_QUERY_CLASS:
            {
                // TODO: remove usage of destination as size provider
                // rcx=size rdx=offset rdi=value rsi=object
                if (isApiScalar(op->data[0]))
                {
                    InsertInteger(op, {1, 8}, -varSize(op->data[0]));
                    InsertInteger(op, {2, 8}, op->data[2]);
                    InsertMove(op, {6, 8}, Register(op->data[1]), false);
                    runtimeApiHeader[HEADER_ENTRY_QUERY_OBJECT].push_back({printCALL(op, 0x0) - assemblyCode, currentOrder});
                    InsertMove(op, Register(op->data[0]), {7, 8}, false);
                }
                else
                {
                    InsertInteger(op, {1, 8}, varSize(op->data[0]));
                    InsertInteger(op, {2, 8}, op->data[2]);
                    InsertInteger(op, {7, 8}, memTable[op->data[0]]);
                    InsertMove(op, {6, 8}, Register(op->data[1]), false);
                    runtimeApiHeader[HEADER_ENTRY_QUERY_OBJECT].push_back({printCALL(op, 0x0) - assemblyCode, currentOrder});
                }
                break;
            }
            
            case OP_QUERY_PIPE:
            {
                // TODO: remove usage of destination as size provider
                // rcx=size rdx=offset rdi=value rsi=object
                if (isApiScalar(op->data[0]))
                {
                    InsertInteger(op, {1, 8}, -varSize(op->data[0]));
                    InsertInteger(op, {2, 8}, 0);
                    InsertMove(op, {6, 8}, Register(op->data[1]), false);
                    runtimeApiHeader[HEADER_ENTRY_QUERY_PIPE].push_back({printCALL(op, 0x0) - assemblyCode, currentOrder});
                    InsertMove(op, Register(op->data[0]), {7, 8}, false);
                }
                else
                {
                    InsertInteger(op, {1, 8}, varSize(op->data[0]));
                    InsertInteger(op, {2, 8}, 0);
                    InsertInteger(op, {7, 8}, memTable[op->data[0]]);
                    InsertMove(op, {6, 8}, Register(op->data[1]), false);
                    runtimeApiHeader[HEADER_ENTRY_QUERY_PIPE].push_back({printCALL(op, 0x0) - assemblyCode, currentOrder});
                }
                break;
            }
             
            case OP_BOR:   ABEL_BINOP(ASM_OR) break;
            case OP_BAND:  ABEL_BINOP(ASM_AND) break;
            case OP_BXOR:  ABEL_BINOP(ASM_XOR) break;

            // TODO: add variant without BMI2
            case OP_SHL:   
                // shlx $0 $1 $2
                printRRR(op, ASM_SHLX, Register(op->data[0]), Register(op->data[1]), Register(op->data[2]));
                break;
            case OP_SHR:
                // shrx $0 $1 $2
                printRRR(op, ASM_SHRX, Register(op->data[0]), Register(op->data[1]), Register(op->data[2]));
                break;
            
            case OP_BNOT:
                InsertMove(op, op->data[0], op->data[2]);
                printR(op, ASM_NOT, Register(op->data[0]));
                break;
            
            case OP_ADD:   ABEL_BINOP(ASM_ADD) break;
            case OP_MUL:   ABEL_BINOP(ASM_IMUL) break;
            case OP_SUB:
                if (regTable[op->data[0]] == regTable[op->data[2]])
                {
                    printRR(op, ASM_SUB, Register(op->data[0]), Register(op->data[1]));
                    printR(op, ASM_NEG, Register(op->data[0]));
                }
                else
                {
                    InsertMove(op, op->data[0], op->data[1]);
                    printRR(op, ASM_SUB, Register(op->data[0]), Register(op->data[2]));
                }
                break;
            
            case OP_EQ:    CMPOP(ASM_SETE,  ASM_SETE)  break;
            case OP_NE:    CMPOP(ASM_SETNE, ASM_SETNE) break;
            case OP_LT:    CMPOP(ASM_SETL,  ASM_SETB)  break;
            case OP_LE:    CMPOP(ASM_SETLE, ASM_SETBE) break;
            case OP_GT:    CMPOP(ASM_SETG,  ASM_SETA)  break;
            case OP_GE:    CMPOP(ASM_SETGE, ASM_SETAE) break;

            case OP_DIV:
                InsertMove(op, {0, 8}, Register(op->data[1]), isSigned(op->data[0]));            // mov rax, $1
                if (isSigned(op->data[0])) 
                    printZ(op, ASM_CQO);                                                         // cqo
                else 
                    printRR(op, ASM_XOR, {2, 4}, {2, 4});                                        // xor edx, edx
                printR(op, (isSigned(op->data[0]) ? ASM_IDIV : ASM_DIV), Register(op->data[2])); // div $2
                InsertMove(op, Register(op->data[0]), {0, 8}, false);                            // mov $0, rax ; sign doesn't matter
                break;
                
            case OP_MOD:
                InsertMove(op, {0, 8}, Register(op->data[1]), isSigned(op->data[0]));            // mov rax, $1
                if (isSigned(op->data[0])) 
                    printZ(op ,ASM_CQO);                                                         // cqo
                else 
                    printRR(op, ASM_XOR, {2, 4}, {2, 4});                                        // xor edx, edx
                printR(op, (isSigned(op->data[0]) ? ASM_IDIV : ASM_DIV), Register(op->data[2])); // div $2
                InsertMove(op, Register(op->data[0]), {2, 8}, false);                            // mov $0, rdx ; sign doesn't matter
                break;
        }

        for (auto &n : views::reverse(op->next))
        {
            toBuild.push_back({n, op});
        }
    }

    void InsertJumpInstructions()
    {        
        // need to select JumpInstructions sizes
        vector<int64_t> shortJmp(JumpInstructions.size(), 0); // use everythere shortest form
        vector<BYTE *> currentPosition(JumpInstructions.size());
        map<OperationBlock *, BYTE *> opPosition;
        map<pair<BYTE *, int64_t>, int64_t> offsets;
        int64_t totalAddSize = 0;


        stable_sort(JumpInstructions.begin(), JumpInstructions.end(), [](const jmpInstruction &a, const jmpInstruction &b){
            return a.codePos < b.codePos || (a.codePos == b.codePos && a.codeOrder < b.codeOrder);
        });


        bool need_next;
        do
        {
            // update positions
            {
                offsets[{NULL, 0}] = 0;
                
                int64_t lastOffset = 0;
                int64_t id = 0;

                // fill offsets map
                for (auto &i : JumpInstructions)
                {
                    currentPosition[id] = i.codePos + lastOffset;
                    lastOffset += JMPsize(i.node, i.jmpType, 0, shortJmp[id]).first;
                    offsets[{i.codePos, i.codeOrder}] = lastOffset;
                    
                    id++;
                }

                // save total size
                totalAddSize = lastOffset;
                
                // fill opcode positions
                opPosition.clear();
                for (auto &[op, addr] : addressTable)
                {
                    int64_t opOffset = prev(offsets.upper_bound({addr, orderTable[op]}))->second;
                    opPosition[op] = addr + opOffset;
                }
            }
            // check if there is any too long jumps
            need_next = false;
            {
                int64_t id = 0;
                for (auto &i : JumpInstructions)
                {
                    int64_t need_variant = JMPsize(i.node, i.jmpType, opPosition[i.destOp] - currentPosition[id], shortJmp[id]).second;
                    if (need_variant != shortJmp[id])
                    {
                        shortJmp[id] = need_variant; // try to use next range level
                        need_next = true;
                    }
                    id++;
                }
            }
        }
        while (need_next);

        // all jumps is now of right size - insert them
        {
            int64_t id = JumpInstructions.size() - 1;
            BYTE *newAssmeblyEnd = assemblyEnd + totalAddSize;
            BYTE *codeDest = assemblyEnd + totalAddSize;
            BYTE *codeSrc = assemblyEnd;
            for (auto &i : views::reverse(JumpInstructions))
            {
                // copy code block
                int64_t blockSize = codeSrc - i.codePos;
                if (blockSize)
                {
                    codeDest -= blockSize;
                    codeSrc  -= blockSize;
                    memmove(codeDest, codeSrc, blockSize);
                }

                printf("inserted %lld to %lld ... [jmp to %lld [+%lld]] [to instruction %p]\n", shortJmp[id], currentPosition[id] - assemblyCode, opPosition[i.destOp] - assemblyCode, opPosition[i.destOp] - currentPosition[id], i.destOp);
                // insert jump instruction
                codeDest -= JMPsize(i.node, i.jmpType, opPosition[i.destOp] - currentPosition[id], shortJmp[id]).first;
                
                assemblyEnd = codeDest;
                
                assert(codeDest - assemblyCode == currentPosition[id] - assemblyCode);
                
                assert(printJMP(i.node, i.jmpType, opPosition[i.destOp] - currentPosition[id], shortJmp[id]) == shortJmp[id]);
                
                id--;
            }

            // update header
            for (auto &[k, v] : runtimeApiHeader)
            {
                for (auto &p : v)
                {                   
                    int64_t opOffset = prev(offsets.upper_bound({assemblyCode + p.position, p.order}))->second;
                    p.position += opOffset;
                }
            }

            // update addressTable
            for (auto &[k, v] : addressTable)
            {
                int64_t opOffset = prev(offsets.upper_bound({v, orderTable[k]}))->second;
                v += opOffset;
            }

            // restore end
            assemblyEnd = newAssmeblyEnd;
        }
    }

    void ExportToFile(const char *filename)
    {
        FILE *f = fopen(filename, "wb");

        BYTE *buf = (BYTE *)malloc(1024 * 1024);
        BYTE *buf_start = buf;
        
        /* generate prefix */
        *buf++ = 'H'; *buf++ = 'I'; *buf++ = 'V'; *buf++ = 'E';
        
        *(uint64_t *)buf = 1; // version 0.1
        buf += 8;
        
        /* generate header */
        *(uint64_t *)buf = 0xBEBEBEBEBEBEBEBE; // version 0.1
        buf += 8;

        /* add relocations */
        for (auto &[id, value] : runtimeApiHeader)
        {
            *buf++ = id;
            *(uint64_t *)buf = value.size();
            buf += 8;
            for (auto &pos : value)
            {
                *(uint64_t *)buf = pos.position;
                buf += 8;
            }
        }

        /* add dll import data */
        {
            for (auto &[id, data] : dllImportHeader)
            {
                printf("Export dllimport data for worker %lld\n", id);
                *buf++ = HEADER_ENTRY_DLL_IMPORT;
                /* export id */
                *(uint64_t *)buf = id;
                buf += 8;
                /* export library name */
                *(uint64_t *)buf = data.library.size();
                buf += 8;
                memcpy(buf, data.library.c_str(), data.library.size());
                buf += data.library.size();
                /* export function name */
                *(uint64_t *)buf = data.entry.size();
                buf += 8;
                memcpy(buf, data.entry.c_str(), data.entry.size());
                buf += data.entry.size();
                /* export output size [base from promise] */
                *(uint64_t *)buf = data.output;
                buf += 8;
                /* export inputs count */
                *(uint64_t *)buf = data.inputs.size();
                buf += 8;
                /* export inputs */
                for (auto &size : data.inputs)
                {
                    *(uint64_t *)buf = size;
                    buf += 8;
                }
            }
        }

        /* add workers positions */
        {
            *buf++ = 16;
            *(uint64_t *)buf = resultWorkerPositions.size();
            buf += 8;
            for (auto &[id, pos] : resultWorkerPositions)
            {
                printf("Export worker %lld with offset %016llx\n", id, pos);
                /* export id */
                *(uint64_t *)buf = id;
                buf += 8;
                /* export position */
                *(uint64_t *)buf = pos;
                buf += 8;
                /* export input table size */
                *(uint64_t *)buf = GetWorkerInputTableSize(id);
                buf += 8;
            }
        }

        /* add string table */
        {
            *buf++ = 17;
            // insert strings count
            *(int64_t *)buf = ir->strings.size();
            buf += 8;
            // insert encoding [0x0=RAW]
            *buf++ = 0x0;
            // push data using encoding:
            int64_t total_size = 0;
            for (auto &k : ir->strings)
            {
                total_size += k.size();
                total_size += (8-(total_size)%8)%8;
            }
            *(int64_t *)buf = total_size;
            buf += 8;
            // table [offset+size] + raw strings
            int64_t of = 0;
            for (auto &k : ir->strings)
            {
                *(int64_t *)buf = of;
                buf += 8;
                *(int64_t *)buf = k.size();
                buf += 8;
                of += k.size();
                of += (8-(of)%8)%8;
            }
            BYTE *begin = buf;
            for (auto &k : ir->strings)
            {
                memcpy(buf, k.data(), k.size());
                buf += k.size();
                buf += (8-(buf-begin)%8)%8;
            }
            printf("Exported %lld strings used in sum %lld bytes\n", ir->strings.size(), of);
        }
        

        /* fill header size */
        *(uint64_t *)(buf_start + 12) = buf - buf_start;
        

        int64_t totalBytes = buf - buf_start + assemblyEnd - assemblyCode;
        fwrite(buf_start, 1, buf - buf_start, f);
        fwrite(assemblyCode, 1, assemblyEnd - assemblyCode, f);

        fclose(f);

        printf("%lld bytes written\n", totalBytes);
    }
};


CodeAssembler *new_x64_win_Assembler()
{
    return new WinX64Assembler();
}
