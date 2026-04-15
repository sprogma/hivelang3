#ifndef CODEGEN_H
#define CODEGEN_H


#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <vector>
#include <algorithm>
#include <array>
#include <ranges>
#include <string>
#include <map>

using namespace std;


#include "../../headers/ir.hpp"
#include "../../headers/logger.hpp"
#include "../../optimization/optimizer.hpp"
#include "../../analysis/analizators.hpp"
#include "../codegen.hpp"
#include "../registerAllocator.hpp"
#include "../memoryAllocator.hpp"


/* ------------------------- assembler enums --------------------- */
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

enum asm_operation5mr
{
    ASM_MOV_5MR,
};
enum asm_operation5rm
{
    ASM_MOV_5RM,
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


/* --------------------------------- internal structures -------------------------- */

struct jmpInstruction
{
    asm_operation_jmp jmpType;
    BYTE *codePos;
    int64_t codeOrder;
    OperationBlock *destOp;
    OperationBlock *node;
};

/* ---------------------------------- main class ---------------------------------- */

struct CodeSpan
{
    int64_t start, end;
    CodeSpan(int64_t start, int64_t end) : start(start), end(end) {}
    CodeSpan(int64_t pos) : start(pos), end(pos + 1) {}
};

class WinX64Assembler : public CodeAssembler
{    
public:
    WinX64Assembler(const map<string, string> &config) : config(config)
    {}

private:
    map<string, string> config;
    map<pair<int64_t, int64_t>, CodeSpan> addrToLine;
    void markAddress(BYTE *from, int64_t count, CodeSpan code)
    {
        addrToLine.insert({{from - assemblyCode, (from - assemblyCode) + count}, code});
    }

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

    
public:
    map<int64_t, int64_t> resultWorkerPositions;
    map<int64_t, int64_t> resultWorkerSize;
    
    pair<BYTE *, BYTE *> Build(BuildResult *input, BYTE *header, BYTE *body, int64_t bodyOffset) override ;
    
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
    map<BYTE, vector<api_call_entry>> runtimeApiHeader;
    
    vector<pair<OperationBlock *, OperationBlock *>> toBuild;

    
    /*
        registers: 
            rbp - pointer on locals
            rdi - pointer on inputs table + used in api calls // TODO: optimize?
            rsi - used in api calls
            rcx - used in api calls
            rax - used for division / api calls
            rdx - used for division / api calls

        [rdi is used in api calls, becouse all api calls will be after LOAD_INPUT]
    */


    // registers configuraion
    static constexpr int64_t registersCount = 9;
    // all other registers
    const int64_t registers[registersCount] = {0b0011, 0b1000, 0b1001, 0b1010, 0b1011, 0b1100, 0b1101, 0b1110, 0b1111};

    // :)
    void printZ(OperationBlock *node, asm_operationZ op);
    void printR(OperationBlock *node, asm_operation1 op, pair<int64_t, int64_t> r1);
    void printRR(OperationBlock *node, asm_operation2 op, pair<int64_t, int64_t> r1, pair<int64_t, int64_t> r2);
    void printRM(OperationBlock *node, asm_operation3rm op, pair<int64_t, int64_t> r1, pair<int64_t, int64_t> r2, int64_t offset);
    void printRRCRC(OperationBlock *node, asm_operation5rm op, pair<int64_t, int64_t> dest, pair<int64_t, int64_t> base, int64_t elementSize, pair<int64_t, int64_t> index, int64_t elementOffset);
    void printRCRCR(OperationBlock *node, asm_operation5mr op, pair<int64_t, int64_t> base, int64_t elementSize, pair<int64_t, int64_t> index, int64_t elementOffset, pair<int64_t, int64_t> source);
    void printRC(OperationBlock *node, asm_operation2rc op, pair<int64_t, int64_t> r1, int64_t value);
    void printMR(OperationBlock *node, asm_operation3mr op, pair<int64_t, int64_t> r1, pair<int64_t, int64_t> r2, int64_t offset);
    void printRRC(OperationBlock *node, asm_operation3rrc op, pair<int64_t, int64_t> r1, pair<int64_t, int64_t> r2, int64_t constant);
    void printRRR(OperationBlock *node, asm_operation3 op, pair<int64_t, int64_t> r1, pair<int64_t, int64_t> r2, pair<int64_t, int64_t> r3);
    pair<int64_t, int64_t> JMPsize(OperationBlock *node, asm_operation_jmp op, int64_t offset, int64_t encoding_variant);
    int64_t printJMP(OperationBlock *node, asm_operation_jmp op, int64_t offset, int64_t encoding_variant);
    BYTE *printCALL(OperationBlock *node, int64_t address);
    bool isSigned(int64_t name);
    bool isApiScalar(int64_t name);
    TypeContext *varType(int64_t name);
    int64_t varSize(int64_t name);
    int64_t varTypeID(TypeContext *type);
    int64_t GetInputOffset(int64_t id);
    int64_t GetWorkerInputTableSize(int64_t id);
    int64_t GetOutputOffset(int64_t id);
    const pair<int64_t, int64_t> Register(int64_t var);
    const pair<int64_t, int64_t> Register(int64_t var, int64_t size);
    void BuildFn(WorkerDeclarationContext *wk, int64_t workerId);
    void ExpandCallInstructions(WorkerDeclarationContext *wk);
    void InsertMove(OperationBlock *node, pair<int64_t, int64_t> dest, pair<int64_t, int64_t> from, bool isSigned);
    void InsertMove(OperationBlock *node, int64_t dest, int64_t from);
    void InsertInteger(OperationBlock *node, pair<int64_t, int64_t> dest, int64_t value);
    void ExternTo64Bit(OperationBlock *node, pair<int64_t, int64_t> reg, bool is_signed);
    int64_t GetClassSize(TypeContext *type);
    void BuildOperation();
    void InsertJumpInstructions();
    pair<BYTE *, BYTE *> ExportToFile(BYTE *header, BYTE *body, int64_t bodyOffset);
};

#endif
