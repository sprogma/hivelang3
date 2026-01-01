#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
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



class WinX64Assembler : public CodeAssembler
{
public:
    WinX64Assembler()
    {}

private:
    map<int64_t, WorkerDeclarationContext *> idToWorker;
    BuildResult *ir;
    char *assemblyCode, *assemblyEnd;
    int64_t assemblyAlloc;
    int64_t nextLabelId;

    __attribute__ ((format (printf, 2, 3)))
    void print(const char *format_string, ...)
    {
        int64_t pos = assemblyEnd - assemblyCode;
        if (pos + 1000 > assemblyAlloc)
        {
            while (pos + 1000 > assemblyAlloc)
            {
                assemblyAlloc = 2 * assemblyAlloc + !assemblyAlloc;
            }
            assemblyCode = (char *)realloc(assemblyCode, assemblyAlloc);
            assemblyEnd = assemblyCode + pos;
        }
        va_list args;
        va_start(args, format_string); 
        assemblyEnd += vsprintf(assemblyEnd, format_string, args);
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
        
        *assemblyEnd++ = '\0';
        puts(assemblyCode);
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

    /*
        registers: 
            rbp - pointer on locals
            rsp - pointer on inputs table // TODO: optimize
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

    void InsertMove(int64_t dest, int64_t from)
    {
        TypeContext *t1 = varType(dest);
        TypeContext *t2 = varType(from);
        if (t1 == t2 || t1->size < t2->size)
        {
            if (regTable[dest] != regTable[from])
            {
                print("\tmov  %s, %s\n", RegisterName(dest), RegisterName(from));
            }
            return;
        }
        // t1->size > t2->size
        if (isSigned(dest))
        {
            print("\tmovsx %s, %s\n", RegisterName(dest), RegisterName(from));
        }
        else
        {
            print("\tmovzx %s, %s\n", RegisterName(dest), RegisterName(from));
        }
    }

    void InsertInteger(int64_t dest, int64_t value)
    {
        switch (value)
        {
            case 0:
                print("\txor  %s, %s\n", RegisterName(dest), RegisterName(dest));
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
                print("\t" T "  %s, %s\n", RegisterName(op->data[2]), RegisterName(op->data[1])); \
            } \
            else \
            { \
                InsertMove(op->data[0], op->data[1]); \
                print("\t" T "  %s, %s\n", RegisterName(op->data[0]), RegisterName(op->data[2])); \
            }
        #define CMPOP(A, B) \
            print("\tcmp  %s, %s\n", RegisterName(op->data[1]), RegisterName(op->data[2])); \
            print("\tmov  %s, 0\n", RegisterName(op->data[0])); \
            if (isSigned(op->data[0])) \
                print("\t" A " %s\n", RegisterName(regTable[op->data[0]], 1)); \
            else \
                print("\t" B " %s\n", RegisterName(regTable[op->data[0]], 1)); \
            print("\tneg  %s\n", RegisterName(op->data[0]));
        
        switch (op->type)
        {
            // impossible
            case OP_JMP: break;    
            // nothing to do
            case OP_FREE_TEMP: break;
            
            case OP_JZ:
                print("\ttest %s, %s\n", RegisterName(op->data[0]), RegisterName(op->data[0]));
                print("\tjz   op_%p\n", op->next[1]);
                break;
                
            case OP_JNZ: 
                print("\ttest %s, %s\n", RegisterName(op->data[0]), RegisterName(op->data[0]));
                print("\tjnz  op_%p\n", op->next[1]);
                break;
        
            case OP_LOAD:
                print("\tmov  %s, %s [rbp + %lld]\n", RegisterName(op->data[0]), SizeQualifer(op->data[0]), memTable[op->data[1]]);
                break;
            case OP_STORE:
                print("\tmov  %s [rbp + %lld], %s\n", SizeQualifer(op->data[0]), memTable[op->data[1]], RegisterName(op->data[0]));
                break;
        
            case OP_LOAD_INPUT: 
                print("\tmov  %s, %s [rsp + %lld]\n", RegisterName(op->data[1]), SizeQualifer(op->data[1]), GetInputOffset(op->data[0]));
                break;
            case OP_LOAD_OUTPUT: 
                print("\tmov  %s, %s [rsp + %lld]\n", RegisterName(op->data[1]), SizeQualifer(op->data[1]), GetOutputOffset(op->data[0]));
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
                print("\tmov %s [rbp + %lld], %s\n", SizeQualifer((TypeContext *)op->data[2]), memTable[op->data[0]] + op->data[1], RegisterName(op->data[3]));
                break;
                
            case OP_PUSH_ARRAY:   ApiCall("push_array"); break;
            case OP_PUSH_PIPE:    ApiCall("push_pipe"); break;
            case OP_PUSH_PROMISE: ApiCall("push_promise"); break;
            case OP_PUSH_CLASS:   ApiCall("push_class"); break;
                
            case OP_QUERY_VAR: 
                assert(op->data[2] != 0 || (TypeContext *)op->data[3] != varType(op->data[0]));
                print("\tmov %s, %s [rbp + %lld]\n", RegisterName(op->data[0]), SizeQualifer((TypeContext *)op->data[3]), memTable[op->data[1]] + op->data[2]);
                break;
                
            case OP_QUERY_ARRAY:   ApiCall("query_array"); break;
            case OP_QUERY_INDEX:   ApiCall("query_index"); break;
            case OP_QUERY_PIPE:    ApiCall("query_pipe"); break;
            case OP_QUERY_PROMISE: ApiCall("query_promise"); break;
            case OP_QUERY_CLASS:   ApiCall("query_class"); break;
             
            case OP_BOR:   ABEL_BINOP("or") break;
            case OP_BAND:  ABEL_BINOP("and") break;
            case OP_BXOR:  ABEL_BINOP("xor") break;

            // TODO: add variant without BMI2
            case OP_SHL:   print("\tshlx %s, %s, %s\n", RegisterName(op->data[0]), RegisterName(op->data[1]), RegisterName(op->data[2])); break;
            case OP_SHR:   print("\tshrx %s, %s, %s\n", RegisterName(op->data[0]), RegisterName(op->data[1]), RegisterName(op->data[2])); break;
            
            case OP_BNOT:        print("\tnot  %s, %s\n", RegisterName(op->data[0]), RegisterName(op->data[2])); break;
            
            case OP_ADD:   ABEL_BINOP("add") break;
            case OP_MUL:   ABEL_BINOP("imul") break;
            case OP_SUB:
                if (regTable[op->data[0]] == regTable[op->data[2]])
                {
                    print("\tsub %s, %s\n", RegisterName(op->data[0]), RegisterName(op->data[1]));
                    print("\tneg %s\n", RegisterName(op->data[0]));
                }
                else
                {
                    InsertMove(op->data[0], op->data[1]);
                    print("\tsub %s, %s\n", RegisterName(op->data[0]), RegisterName(op->data[2]));
                }
                break;
            
            case OP_EQ:    CMPOP("sete", "sete") break;
            case OP_NE:    CMPOP("setne", "setne") break;
            case OP_LT:    CMPOP("setl", "setb") break;
            case OP_LE:    CMPOP("setle", "setbe") break;
            case OP_GT:    CMPOP("setg", "seta") break;
            case OP_GE:    CMPOP("setge", "setae") break;

            case OP_DIV:
                print("\tmov  rax, %s\n", RegisterName(op->data[1]));
                if (isSigned(op->data[0])) 
                    print("\tcqo\n");
                else 
                    print("\txor  edx, edx\n");
                print("\tdiv  %s\n", RegisterName(op->data[2])); 
                print("\tmov  %s, rax", RegisterName(op->data[0]));
                break;
                
            case OP_MOD:
                print("\tmov  rax, %s\n", RegisterName(op->data[1]));
                if (isSigned(op->data[0])) 
                    print("\tcqo\n");
                else 
                    print("\txor  edx, edx\n");
                print("\tdiv  %s\n", RegisterName(op->data[2])); 
                print("\tmov  %s, rdx", RegisterName(op->data[0]));
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
