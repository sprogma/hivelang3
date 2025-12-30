#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <math.h>
#include <vector>
#include <bitset>
#include <array>
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
        print("op_%p:\n", op);
        // if instruction have many next:
        #define BINOP \
            print("\tmov  %s, %s\n", RegisterName(op->data[0]), RegisterName(op->data[1]));
        #define CMPOP(A, B) \
            print("\txor  %s, %s\n", RegisterName(op->data[0]), RegisterName(op->data[0])); \
            print("\tcmp  %s, %s\n", RegisterName(op->data[1]), RegisterName(op->data[2])); \
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
                print("\ttest %s\n", RegisterName(op->data[0]));
                print("\tjz   op_%p\n", op->next[1]);
                break;
                
            case OP_JNZ: 
                print("\ttest %s\n", RegisterName(op->data[0]));
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
                print("\tmov  %s, %s\n", RegisterName(op->data[0]), RegisterName(op->data[1]));
                break;
                
            case OP_MOV: 
                print("\tmov  %s, %s\n", RegisterName(op->data[0]), RegisterName(op->data[1]));
                break;

            case OP_NEW_INT: 
                print("\tmov  %s, %lld\n", RegisterName(op->data[0]), op->data[1]);
                break;
                
            case OP_NEW_FLOAT:
                print("\tOP_NEW_FLOAT [not supported]\n"); 
                break;
            case OP_NEW_ARRAY:    ApiCall("new_array"); break;
            case OP_NEW_PIPE:     ApiCall("new_pipe"); break;
            case OP_NEW_PROMISE:  ApiCall("new_promise"); break;
            case OP_NEW_CLASS:    ApiCall("new_class"); break;
            
            case OP_PUSH_VAR: 
                if (op->data.size() == 2)
                {
                    print("\tmov %s, %s\n", RegisterName(op->data[0]), RegisterName(op->data[1])); 
                }
                else
                {
                    print("\tOP_PUSH_VAR to structure [not supported]\n"); 
                }
                break;
                
            case OP_PUSH_ARRAY:   ApiCall("push_array"); break;
            case OP_PUSH_PIPE:    ApiCall("push_pipe"); break;
            case OP_PUSH_PROMISE: ApiCall("push_promise"); break;
            case OP_PUSH_CLASS:   ApiCall("push_class"); break;
                
            case OP_QUERY_VAR: 
                if (op->data.size() == 2)
                {
                    print("\tmov  %s, %s\n", RegisterName(op->data[0]), RegisterName(op->data[1])); 
                }
                else
                {
                    print("\tOP_QUERY_VAR from structure [not supported]\n"); 
                }
                break;
                
            case OP_QUERY_ARRAY:   ApiCall("query_array"); break;
            case OP_QUERY_INDEX:   ApiCall("query_index"); break;
            case OP_QUERY_PIPE:    ApiCall("query_pipe"); break;
            case OP_QUERY_PROMISE: ApiCall("query_promise"); break;
            case OP_QUERY_CLASS:   ApiCall("query_class"); break;
             
            case OP_BOR:   BINOP print("\tor   %s, %s\n", RegisterName(op->data[0]), RegisterName(op->data[2])); break;
            case OP_BAND:  BINOP print("\tand  %s, %s\n", RegisterName(op->data[0]), RegisterName(op->data[2])); break;
            case OP_BXOR:  BINOP print("\txor  %s, %s\n", RegisterName(op->data[0]), RegisterName(op->data[2])); break;
            case OP_SHL:   BINOP print("\tshl  %s, %s\n", RegisterName(op->data[0]), RegisterName(op->data[2])); break;
            case OP_SHR:   BINOP print("\tshr  %s, %s\n", RegisterName(op->data[0]), RegisterName(op->data[2])); break;
            
            case OP_BNOT:        print("\tnot  %s, %s\n", RegisterName(op->data[0]), RegisterName(op->data[2])); break;
            
            case OP_ADD:   BINOP print("\tadd  %s, %s\n", RegisterName(op->data[0]), RegisterName(op->data[2])); break;
            case OP_SUB:   BINOP print("\tsub  %s, %s\n", RegisterName(op->data[0]), RegisterName(op->data[2])); break;
            case OP_MUL:   BINOP print("\timul %s, %s\n", RegisterName(op->data[0]), RegisterName(op->data[2])); break;
            
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
