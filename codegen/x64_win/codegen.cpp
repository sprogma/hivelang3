#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <vector>
#include <map>

using namespace std;


#include "../../ir.hpp"
#include "../codegen.hpp"

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
        for (auto &[fn, id] : ir->workers)
        {
            idToWorker[id] = fn;
        }
        
        for (auto &[fn, id] : ir->workers)
        {
            BuildFn(fn, id);
        }

        // add terminating zero
        *assemblyEnd++ = '\0';
        
        printf("Code generated.\n");
        puts(assemblyCode);
    }

private:
    set<OperationBlock *> generated;
    map<int64_t, pair<int64_t, int64_t>> memoryImage;
    int64_t usedMemory;
    
    const int64_t registersCount = 4;
    
    vector<int64_t> registersUsed; // id of loaded variable
    WorkerDeclarationContext *current;

    const char *SizeQualifer(int64_t size)
    {
        switch (size)
        {
            case 8: return "QWORD PTR";
            case 4: return "DWORD PTR";
            case 2: return "WORD PTR";
            case 1: return "BYTE PTR";
        }
        return "";
    }

    const char *RegisterName(pair<int64_t, int64_t> reg)
    {
        switch (reg.second)
        {
            case 8: return (vector<const char *>{"rax",  "r10",  "r11",  "r12"})[reg.first];
            case 4: return (vector<const char *>{"eax", "r10d", "r11d", "r12d"})[reg.first];
            case 2: return (vector<const char *>{ "ax", "r10w", "r11w", "r12w"})[reg.first];
            case 1: return (vector<const char *>{ "al", "r10b", "r11b", "r12b"})[reg.first];
        }
        return "";
    }

    void BuildFn(WorkerDeclarationContext *wk, int64_t workerId)
    {
        usedMemory = 0;
        memoryImage.clear();
        registersUsed = vector<int64_t>(registersCount, -1);
        current = wk;
        
        print("worker_%lld:\n", workerId);
        if (wk->content == NULL)
        {
            printf("For now, x64-win doesn't support dynamic linking\n");
        }
        else
        {
            BuildOperation(wk->content->entry);
        }
    }

    void AllocateMemory(int64_t name)
    {
        // TODO: write better allocator
        TypeContext *type = current->content->variables[name];
        int64_t start = usedMemory;
        int64_t end = usedMemory + type->size;
        memoryImage[name] = {start, end};
        usedMemory += type->size;
    }

    int64_t GetVariableSize(int64_t name)
    {
        return current->content->variables[name]->size;
    }

    void UnloadVariable(int64_t reg)
    {
        registersUsed[reg] = -1;
        int64_t var = registersUsed[reg];
        if (memoryImage.find(var) == memoryImage.end())
        {
            AllocateMemory(var);
        }
        int64_t size = GetVariableSize(var);
        print("\tmov %s [rbp+%lld], %s\n", 
              SizeQualifer(size), 
              memoryImage[var].first, 
              RegisterName({reg, size}));
    }

    pair<int64_t, int64_t> LoadVariable(int64_t name, int64_t reg, bool read=true)
    {
        registersUsed[reg] = name;
        if (memoryImage.find(name) == memoryImage.end() || read == false)
        {
            return {reg, GetVariableSize(name)};
        }
        int64_t size = GetVariableSize(name);
        print("\tmov %s, %s [rbp+%lld]\n", 
              RegisterName({reg, size}),
              SizeQualifer(size),
              memoryImage[name].first);
        return {reg, size};
    }
    
    pair<int64_t, int64_t> AcqureVariable(int64_t name, set<int64_t> fix, bool read=true)
    {
        /* if it was allocated - return it */
        for (int64_t i = 0; i < (int64_t)registersUsed.size(); ++i)
        {
            if (registersUsed[i] == name) return {i, GetVariableSize(name)};
        }
        /* if there is free cell - return it */
        for (int64_t i = 0; i < (int64_t)registersUsed.size(); ++i)
        {
            if (registersUsed[i] == -1) return LoadVariable(name, i, read);
        }
        /* unload random not fixed variable */
        vector<int64_t> variants;
        for (int64_t i = 0; i < (int64_t)registersUsed.size(); ++i)
        {
            if (fix.find(registersUsed[i]) == fix.end()) variants.push_back(i);
        }
        // TODO: better selection algo
        int64_t reg = variants[rand() % variants.size()];
        UnloadVariable(reg);
        return LoadVariable(name, reg, read);
    }

    void BuildOperation(OperationBlock *op)
    {
        if (generated.find(op) != generated.end())
        {
            print("\tjmp op_%p\n", op);
            return;
        }
        
        generated.insert(op);
        
        print("op_%p:\n", op);
        if (IS_JUMP(op->type))
        {
            assert(op->next.size() == 2);
            switch (op->type)
            {
                case OP_JZ:
                {
                    auto reg = AcqureVariable(op->data[0], {});
                    print("\ttest %s\n", RegisterName(reg));
                    print("\tjz op_%p\n", op->next[1]);
                    break;
                }
                case OP_JNZ:
                {
                    auto reg = AcqureVariable(op->data[0], {});
                    print("\ttest %s\n", RegisterName(reg));
                    print("\tjnz op_%p\n", op->next[1]);
                    break;
                }
                default: {}
            }
            BuildNextOperation(op);
            BuildOperation(op->next[1]);
        }
        else
        {
            switch (op->type)
            {
                case OP_JZ: case OP_JNZ: case OP_JMP: break;    
            
                case OP_LOAD_INPUT: printf("OP_LOAD_INPUT [not supported]"); break;
                case OP_LOAD_OUTPUT: printf("OP_LOAD_OUTPUT [not supported]"); break;
                
                case OP_FREE_TEMP: printf("OP_FREE_TEMP [not supported]"); break;
                
                case OP_CALL: printf("OP_CALL [not supported]"); break;
                case OP_CAST: printf("OP_CAST [not supported]"); break;
                case OP_MOV: printf("OP_MOV [not supported]"); break;

                case OP_NEW_INT: printf("OP_NEW_INT [not supported]"); break;
                case OP_NEW_FLOAT: printf("OP_NEW_FLOAT [not supported]"); break;
                case OP_NEW_ARRAY: printf("OP_NEW_ARRAY [not supported]"); break;
                case OP_NEW_PIPE: printf("OP_NEW_PIPE [not supported]"); break;
                case OP_NEW_PROMISE: printf("OP_NEW_PROMISE [not supported]"); break;
                case OP_NEW_CLASS: printf("OP_NEW_CLASS [not supported]"); break;
                
                case OP_PUSH_VAR: printf("OP_PUSH_VAR [not supported]"); break;
                case OP_PUSH_ARRAY: printf("OP_PUSH_ARRAY [not supported]"); break;
                case OP_PUSH_PIPE: printf("OP_PUSH_PIPE [not supported]"); break;
                case OP_PUSH_PROMISE: printf("OP_PUSH_PROMISE [not supported]"); break;
                case OP_PUSH_CLASS: printf("OP_PUSH_CLASS [not supported]"); break;
                case OP_QUERY_VAR: printf("OP_QUERY_VAR [not supported]"); break;
                case OP_QUERY_ARRAY: printf("OP_QUERY_ARRAY [not supported]"); break;
                case OP_QUERY_INDEX: printf("OP_QUERY_INDEX [not supported]"); break;
                case OP_QUERY_PIPE: printf("OP_QUERY_PIPE [not supported]"); break;
                case OP_QUERY_PROMISE: printf("OP_QUERY_PROMISE [not supported]"); break;
                case OP_QUERY_CLASS: printf("OP_QUERY_CLASS [not supported]"); break;
                
                case OP_BOR: printf("OP_BOR [not supported]"); break;
                case OP_BAND: printf("OP_BAND [not supported]"); break;
                case OP_BXOR: printf("OP_BXOR [not supported]"); break;
                case OP_SHL: printf("OP_SHL [not supported]"); break;
                case OP_SHR: printf("OP_SHR [not supported]"); break;
                case OP_BNOT: printf("OP_BNOT [not supported]"); break;
                
                case OP_ADD:
                {
                    auto v0 = AcqureVariable(op->data[0], {}, false);
                    auto v1 = AcqureVariable(op->data[1], {});
                    auto v2 = AcqureVariable(op->data[2], {});
                    print("\txor %s, %s\n", RegisterName({v0.first, 8}), RegisterName({v0.first, 8}));
                    print("\tadd %s, %s\n", RegisterName({v0.first, 8}), RegisterName({v1.first, 8}));
                    print("\tadd %s, %s\n", RegisterName({v0.first, 8}), RegisterName({v2.first, 8}));
                    break;
                }
                case OP_SUB: printf("OP_SUB [not supported]"); break;
                case OP_MUL: printf("OP_MUL [not supported]"); break;
                case OP_DIV: printf("OP_DIV [not supported]"); break;
                case OP_MOD: printf("OP_MOD [not supported]"); break;
                
                case OP_EQ: printf("OP_EQ [not supported]"); break;
                case OP_LT: printf("OP_LT [not supported]"); break;
                case OP_LE: printf("OP_LE [not supported]"); break;
                case OP_GT: printf("OP_GT [not supported]"); break;
                case OP_GE: printf("OP_GE [not supported]"); break;
            }
            
            assert(op->next.size() == 1);
            BuildNextOperation(op);
        }
    }

    void BuildNextOperation(OperationBlock *op)
    {   
        if (op->next[0] == NULL)
        {
            print("\t[ret?]\n");
        }
        else
        {
            BuildOperation(op->next[0]);
        }
    }
};


CodeAssembler *new_x64_win_Assembler()
{
    return new WinX64Assembler();
}
