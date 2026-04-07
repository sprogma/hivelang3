#include <algorithm>
#include "codegen.hpp"


// more translation tables


void WinX64Assembler::printZ(OperationBlock *node, asm_operationZ op)
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

void WinX64Assembler::printR(OperationBlock *node, asm_operation1 op, pair<int64_t, int64_t> r1)
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


void WinX64Assembler::printRR(OperationBlock *node, asm_operation2 op, pair<int64_t, int64_t> r1, pair<int64_t, int64_t> r2)
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

void WinX64Assembler::printRM(OperationBlock *node, asm_operation3rm op, pair<int64_t, int64_t> r1, pair<int64_t, int64_t> r2, int64_t offset)
{
    if (r2.second != 8)
    {
        logError(ir->filename, ir->code, node->code_start, node->code_end, "Mem register must be 64 bit");
    }
    
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

void WinX64Assembler::printRRCRC(OperationBlock *node, asm_operation5rm op, pair<int64_t, int64_t> dest, pair<int64_t, int64_t> base, int64_t elementSize, pair<int64_t, int64_t> index, int64_t elementOffset)
{
    (void)node;
    
    if (elementSize != 1 && elementSize != 2 && elementSize != 4 && elementSize != 8) {
        logError(ir->filename, ir->code, node->code_start, node->code_end, "Invalid element size for index scaling");
        return;
    }
    
    if (base.second != 8 || index.second != 8) {
        logError(ir->filename, ir->code, node->code_start, node->code_end, "Base and index must be 64 bit");
        return;
    }
    
    int scale = 31 - __builtin_clz(elementSize);
    BYTE rex = 0x40;
    bool needrex = false;
    if (dest.second == 8) { needrex = true; rex |= 0x08; }
    if (dest.first & 8) { needrex = true; rex |= 0x04; }
    if (index.first & 8) { needrex = true; rex |= 0x02; }
    if (base.first & 8) { needrex = true; rex |= 0x01; }
    if (dest.second == 1 && dest.first >= 4) { needrex = true; }

    switch (op)
    {
        case ASM_MOV_5RM:
        {
                
            if (dest.second == 2) { pbyte(0x66); }
            if (needrex) { pbyte(rex); }
            
            BYTE mod = 0;
            bool d8 = false;
            bool d32 = false;
            
            if (elementOffset == 0 && (base.first & 7) != 5) 
            {
                mod = 0; // [base + index*scale]
            } 
            else if (elementOffset >= -128 && elementOffset <= 127) 
            {
                mod = 1; // [base + index*scale + i8]
                d8 = true;
            } 
            else 
            {
                mod = 2; // [base + index*scale + i32]
                d32 = true;
            }

            if ((base.first & 7) == 5 && mod == 0) 
            {
                mod = 1;
                d8 = true;
                elementOffset = 0;
            }
            
            pbyte(dest.second == 1 ? 0x8A : 0x8B);
            
            BYTE modrm = (mod << 6) | ((dest.first & 7) << 3) | 0x04;
            pbyte(modrm);
            
            BYTE sib = (scale << 6) | ((index.first & 7) << 3) | (base.first & 7);
            pbyte(sib);
            
            if (d8) 
            {
                int8_t disp8 = (int8_t)elementOffset;
                pbyte(disp8);
            } 
            else if (d32) 
            {
                int32_t disp32 = (int32_t)elementOffset;
                memcpy(assemblyEnd, &disp32, 4);
                assemblyEnd += 4;
            }
            break;
        }
    }
}   
void WinX64Assembler::printRCRCR(OperationBlock *node, asm_operation5mr op, pair<int64_t, int64_t> base, int64_t elementSize, pair<int64_t, int64_t> index, int64_t elementOffset, pair<int64_t, int64_t> source)
{
    (void)node;
    
    if (elementSize != 1 && elementSize != 2 && elementSize != 4 && elementSize != 8) {
        logError(ir->filename, ir->code, node->code_start, node->code_end, "Invalid element size for index scaling");
        return;
    }
    
    if (base.second != 8 || index.second != 8) {
        logError(ir->filename, ir->code, node->code_start, node->code_end, "Base and index must be 64 bit");
        return;
    }
    
    int scale = 31 - __builtin_clz(elementSize);
    BYTE rex = 0x40;
    bool needrex = false;
    if (source.second == 8) { needrex = true; rex |= 0x08; }
    if (source.first & 8) { needrex = true; rex |= 0x04; }
    if (index.first & 8) { needrex = true; rex |= 0x02; }
    if (base.first & 8) { needrex = true; rex |= 0x01; }
    if (source.second == 1 && source.first >= 4) { needrex = true; }

    switch (op)
    {
        case ASM_MOV_5MR:
        {
            
            if (source.second == 2) { pbyte(0x66); }
            if (needrex) { pbyte(rex); }
            
            BYTE mod = 0;
            bool d8 = false;
            bool d32 = false;
            
            if (elementOffset == 0 && (base.first & 7) != 5) 
            {
                mod = 0; // [base + index*scale]
            } 
            else if (elementOffset >= -128 && elementOffset <= 127) 
            {
                mod = 1; // [base + index*scale + i8]
                d8 = true;
            } 
            else 
            {
                mod = 2; // [base + index*scale + i32]
                d32 = true;
            }

            if ((base.first & 7) == 5 && mod == 0) 
            {
                mod = 1;
                d8 = true;
                elementOffset = 0;
            }
            
            pbyte(source.second == 1 ? 0x88 : 0x89);
            
            BYTE modrm = (mod << 6) | ((source.first & 7) << 3) | 0x04;
            pbyte(modrm);
            
            BYTE sib = (scale << 6) | ((index.first & 7) << 3) | (base.first & 7);
            pbyte(sib);
            
            if (d8) 
            {
                int8_t disp8 = (int8_t)elementOffset;
                pbyte(disp8);
            } 
            else if (d32) 
            {
                int32_t disp32 = (int32_t)elementOffset;
                memcpy(assemblyEnd, &disp32, 4);
                assemblyEnd += 4;
            }
            break;
        }
    }
    
}

void WinX64Assembler::printRC(OperationBlock *node, asm_operation2rc op, pair<int64_t, int64_t> r1, int64_t value)
{
    (void)node;

    bool use32signex = false;
                
    switch (op)
    {
        case ASM_MOV_RC:
            if (r1.second == 8 && value == (uint32_t)value) { r1.second = 4; }
            if (r1.second == 8 && value == (int32_t)value) { use32signex = true; }
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

            if (use32signex)
            {
                pbyte(0xC7);
                pbyte(0xC0 | (r1.first & 7));
                memcpy(assemblyEnd, &value, 4);
                assemblyEnd += 4;
            }
            else
            {
                pbyte((r1.second == 1 ? 0xB0 : 0xB8) | (r1.first & 7));
                memcpy(assemblyEnd, &value, r1.second);
                assemblyEnd += r1.second;
            }
            
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
                memcpy(assemblyEnd, &value, min<int64_t>(4LL, r1.second));
                assemblyEnd += min<int64_t>(4LL, r1.second);
            }
            else
            {
                logError(ir->filename, ir->code, node->code_start, node->code_end, "Can't use ADD_RC with 64bit constant");
                return;
            }
            
            break;
        }
    }
}

void WinX64Assembler::printMR(OperationBlock *node, asm_operation3mr op, pair<int64_t, int64_t> r1, pair<int64_t, int64_t> r2, int64_t offset)
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

void WinX64Assembler::printRRC(OperationBlock *node, asm_operation3rrc op, pair<int64_t, int64_t> r1, pair<int64_t, int64_t> r2, int64_t constant)
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
                logError(ir->filename, ir->code, node->code_start, node->code_end, "IMUL can't take byte variables");
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
                logError(ir->filename, ir->code, node->code_start, node->code_end, "IMUL can't take 64bit constants");
                return;
            }
            break;
    }
}

void WinX64Assembler::printRRR(OperationBlock *node, asm_operation3 op, pair<int64_t, int64_t> r1, pair<int64_t, int64_t> r2, pair<int64_t, int64_t> r3)
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
pair<int64_t, int64_t> WinX64Assembler::JMPsize(OperationBlock *node, asm_operation_jmp op, int64_t offset, int64_t encoding_variant)
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
        logError(ir->filename, ir->code, node->code_start, node->code_end, "Wrong encoding_variant: %lld", encoding_variant);
        return {0, 0};
    }
}

int64_t WinX64Assembler::printJMP(OperationBlock *node, asm_operation_jmp op, int64_t offset, int64_t encoding_variant)
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
        logError(ir->filename, ir->code, node->code_start, node->code_end, "Wrong encoding_variant: %lld", var);
        return -1;
    }
}

BYTE *WinX64Assembler::printCALL(OperationBlock *node, int64_t address)
{
    (void)node;
    
    pbyte(0x48, 0xB8); // mov rax im64
    BYTE *res = assemblyEnd;
    memcpy(assemblyEnd, &address, 8);
    assemblyEnd += 8;
    pbyte(0xFF, 0xD0); // call rax
    return res;
}


