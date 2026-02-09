#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <vector>
#include <variant>
#include <algorithm>
#include <array>
#include <functional>
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


class WinGPUAssembler : public CodeAssembler
{
public:
    WinGPUAssembler()
    {}

private:
    map<int64_t, WorkerDeclarationContext *> idToWorker;
    BuildResult *ir;
    BYTE *assemblyCode, *assemblyEnd;
    int64_t assemblyAlloc;
    int64_t nextLabelId;

    __attribute__ ((format (printf, 2, 3)))
    void print(const char *format_string, ...)
    {
        va_list args;
        va_start(args, format_string); 
        assemblyEnd += vsprintf((char *)assemblyEnd, format_string, args);
        va_end(args);
    }
    
public:
    map<int64_t, pair<int64_t, int64_t>> resultWorkerPositions;
    map<int64_t, vector<pair<int64_t, BYTE>>> resultConfigVariables;
    
    pair<BYTE *, BYTE *> Build(BuildResult *input, BYTE *header, BYTE *body, int64_t bodyOffset) override 
    {
        ir = input;
        /* init build context */
        nextLabelId = 0;
        addressTable.clear();

        /* initializate string */
        assemblyAlloc = 0;
        assemblyCode = assemblyEnd = (BYTE *)malloc(1024 * 1024);

        /* build each worker */
        for (auto &[fn, id] : ir->workers) { idToWorker[id] = fn; }
        for (auto &[fn, id] : ir->workers) { if (fn->used_providers.contains("gpu")){BuildFn(fn, id);} }

        // add terminating zero
        if (!assemblyEnd)
        {
            return {NULL, NULL};
        }

        printf("%s\n", assemblyCode);

        return ExportToFile(header, body, bodyOffset);
    }

private:
    WorkerDeclarationContext *current;
    // registers table
    map<int64_t, int64_t> regTable;
    map<int64_t, int64_t> memTable;
    map<OperationBlock *, BYTE *> addressTable;
    map<OperationBlock *, int64_t> orderTable;

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

    enum argumentType
    {
        ARGUMENT_INDEX = 0x10,
        ARGUMENT_SIZE = 0x20,
        ARGUMENT_OFFSET = 0x30
    };

    void BuildFn(WorkerDeclarationContext *wk, int64_t workerId)
    {
        if (wk->content == NULL) 
        { 
            logError(ir->filename, ir->code, wk->code_start, wk->code_end, "gpu doesn't supports workers without body");
            return;
        }
        printf("Building worker %s\n", wk->name.c_str());
        
        current = wk;

        dumpIR(wk);

        // generate code
        int64_t start = assemblyEnd - assemblyCode;
        
        print("__kernel void krnl(");
        
        vector<pair<int64_t, BYTE>> configVariables(wk->inputs.size());
        map<string, int64_t> inputs;
        map<string, int64_t> offsets;
        int64_t inputId = 0, offset = 0;
        for (auto &[name, type] : wk->inputs)
        {
            offsets[name] = offset;
            inputs[name] = inputId++;
            offset += type->size;
        }
        

        // load id
        {
            string name;
            vector<tuple<string, int64_t, function<void(string, int64_t, int64_t)>>> gpuAttrs = {
                // TODO: may be not long, but printType(???)
                {"GPUxVariable", ARGUMENT_INDEX, [&](string name, int64_t id, int64_t xyzId){(void)name;
                    print("long input_%lld=get_global_id(%lld);", id, xyzId);
                }},
                {"GPUxBase", ARGUMENT_OFFSET, [&](string name, int64_t id, int64_t xyzId){(void)name;
                    print("long input_%lld=get_global_offset(%lld);", id, xyzId);
                }},
                {"GPUxSize", ARGUMENT_SIZE, [&](string name, int64_t id, int64_t xyzId){(void)name;
                    print("long input_%lld=get_global_size(%lld);", id, xyzId);
                }}
            };
            for (auto &[name, basetype, function] : gpuAttrs)
            {
                int64_t xyz = 0;
                for (auto c : vector<char>{'x', 'y', 'z'})
                {
                    name[3] = c;
                    if (wk->attributes.contains(name))
                    {
                        if (!holds_alternative<string>(wk->attributes[name]))
                        {
                            logError(ir->filename, ir->code, wk->code_start, wk->code_end, "%s attribute must be string", name.c_str());
                        }
                        else
                        {
                            int64_t varId = inputs[get<string>(wk->attributes[name])];
                            if (wk->inputs[varId].second->size != 8 || wk->inputs[varId].second->type != TYPE_SCALAR || wk->inputs[varId].second->_scalar.kind != SCALAR_I64)
                            {
                                logError(ir->filename, ir->code, wk->code_start, wk->code_end, "%s attribute variable must be SIGNED 64bit INTEGER", name.c_str());
                            }
                            offset = offsets[get<string>(wk->attributes[name])];
                            configVariables[varId] = {8, basetype | xyz};
                        }
                    }
                    xyz++;
                }
            }
            
            bool not_first = false;
            inputId = 0;
            for (auto &[name, type] : wk->inputs)
            {
                int64_t varId = inputs[name];
                if (!configVariables[varId].second)
                {
                    configVariables[inputId] = {type->size, 0};
                    if (not_first) print(",");
                    printType(NULL, type);
                    print(" input_%lld", varId);
                    not_first = true;
                }
                inputId++;
            }
            print("){");
            
            for (auto &[name, basetype, function] : gpuAttrs)
            {
                int64_t xyz = 0;
                for (auto c : vector<char>{'x', 'y', 'z'})
                {
                    name[3] = c;
                    if (wk->attributes.contains(name) && holds_alternative<string>(wk->attributes[name]))
                    {
                        int64_t varId = inputs[get<string>(wk->attributes[name])];
                        function(name, varId, xyz);
                    }
                    xyz++;
                }
            }
        }
        
        // generate variables
        for (auto &[id, type] : wk->content->variables)
        {
            printType(NULL, type);print(" var_%lld;", id);
        }
        
        
        addressTable.clear();
        orderTable.clear();


        toBuild.push_back({wk->content->entry, NULL});
        while (!toBuild.empty())
        {
            BuildOperation();
        }
        print("}");
        
        int64_t end = assemblyEnd - assemblyCode;
        resultWorkerPositions[workerId] = {start, end};
        resultConfigVariables[workerId] = configVariables;
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

    void printType(OperationBlock *op, TypeContext *type)
    {
        switch (type->type)
        {
            case TYPE_CLASS:   print("void __global *"); break;
            case TYPE_RECORD:  print("struct st_%p", type); break;
            case TYPE_UNION:   print("union st_%p", type); break;
            case TYPE_ARRAY:   printType(op, type->_vector.base); print(" __global *"); break;
            case TYPE_PROMISE: printType(op, type->_vector.base); print(" __global *"); break;
            case TYPE_PIPE:    if (op){logError(ir->filename, ir->code, op->code_start, op->code_end, "Pipe are unsupported in gpu provider");} break;
            case TYPE_SCALAR: 
            {
                switch (type->_scalar.kind)
                {
                    case SCALAR_F32: print("float"); break;
                    case SCALAR_F64: print("double"); break;
                    case SCALAR_I8: print("char"); break;
                    case SCALAR_I16: print("short"); break;
                    case SCALAR_I32: print("int"); break;
                    case SCALAR_I64: print("long"); break;
                    case SCALAR_U8: print("uchar"); break;
                    case SCALAR_U16: print("ushort"); break;
                    case SCALAR_U32: print("uint"); break;
                    case SCALAR_U64: print("ulong"); break;
                }
                break;
            }
        }
    }

    void BuildOperation()
    {
        OperationBlock *op = toBuild.back().first;
        toBuild.pop_back();
        
        // return from function
        if (op == NULL)
        {
            print("return;");
            return;
        }

        int64_t currentOrder = orderTable.size();
        
        if (!addressTable.insert({op, assemblyEnd}).second) 
        { 
            print("goto lb_%p;", op);
            return;
        }

        print("lb_%p:", op);
        orderTable[op] = currentOrder;
        
        switch (op->type)
        {
            // impossible
            case OP_JMP: break;    
            case OP_LOAD:
            case OP_STORE:
            // unsupported for now
            case OP_STORE_INPUT: 
            case OP_CALL:
                logError(ir->filename, ir->code, op->code_start, op->code_end, "calls are unsupported in gpu provider");
                break;
            // nothing to do
            case OP_FREE_TEMP: break;
            
            case OP_JZ:  print("if(var_%lld==0){goto pb_%p;}", op->data[0], op->next[1]); break;
            case OP_JNZ: print("if(var_%lld!=0){goto pb_%p;}", op->data[0], op->next[1]); break;
        
            case OP_LOAD_INPUT:  print("var_%lld=input_%lld;", op->data[1], op->data[0]); break;
            
            // nothing to do
            case OP_LOAD_OUTPUT: break;
                
            case OP_CAST: print("var_%lld=(", op->data[0]); printType(op, varType(op->data[0])); print(")var_%lld;", op->data[1]); break;
            
            case OP_MOV: print("var_%lld=var_%lld;", op->data[0], op->data[1]); break;

            case OP_NEW_INT: 
                if (isSigned(op->data[0])) { print("var_%lld=%lld;", op->data[0], op->data[1]); }
                else                       { print("var_%lld=%llu;", op->data[0], op->data[1]); }
                break;
                
            case OP_NEW_FLOAT:
                if (isSigned(op->data[0])) { print("var_%lld=%lf;", op->data[0], *(double *)&op->data[1]); }
                else                       { print("var_%lld=%lf;", op->data[0], *(double *)&op->data[1]); }
                break;
                
            case OP_NEW_STRING:
            case OP_NEW_ARRAY:
            case OP_NEW_PROMISE:
            case OP_NEW_CLASS:
            case OP_NEW_PIPE:
            {
                logError(ir->filename, ir->code, op->code_start, op->code_end, "new operator is unsupported in gpu provider");
                break;
            }
                
            case OP_PUSH_VAR:
            {
                print("*(");printType(op, varType(op->data[3]));print("*)((char *)&var_%lld+%lld)=var_%lld;", op->data[0], op->data[1], op->data[3]);
                break;
            }                
            case OP_PUSH_ARRAY:
            {
                print("*(");printType(op, varType(op->data[4]));print("*)((char*)var_%lld+var_%lld*%lld+%lld)=var_%lld;", op->data[0], op->data[1], varType(op->data[0])->_vector.base->size, op->data[2], op->data[4]);
                break;
            }
            case OP_PUSH_PROMISE:
            {
                print("*(");printType(op, varType(op->data[1]));print("*)var_%lld=var_%lld;", op->data[0], op->data[1]);
                break;
            }
            case OP_PUSH_PIPE:
            {
                logError(ir->filename, ir->code, op->code_start, op->code_end, "pipes are unsupported in gpu provider");
                break;
            }
            case OP_PUSH_CLASS:
            {
                print("*(");printType(op, varType(op->data[3]));print("*)((char *)var_%lld+%lld)=var_%lld;", op->data[0], op->data[1], op->data[3]);
                break;
            }                
            case OP_QUERY_VAR: 
            {
                print("var_%lld=*(", op->data[0]);printType(op, varType(op->data[0]));print("*)((char *)&var_%lld+%lld);", op->data[1], op->data[2]);
                break;
            }
            case OP_QUERY_INDEX:
            {
                print("var_%lld=*(", op->data[0]);printType(op, varType(op->data[0]));print("*)((char *)var_%lld+var_%lld*%lld+%lld);", op->data[1], op->data[4], varType(op->data[1])->_vector.base->size, op->data[2]);
                break;
            }
            case OP_QUERY_ARRAY:
            {
                logError(ir->filename, ir->code, op->code_start, op->code_end, "for now, query of array is unsupported in gpu provider");
                // print("var_%lld=var_%lld_size", op->data[0], op->data[1]);
                break;
            }
            case OP_QUERY_PROMISE:
            {
                print("var_%lld=*(", op->data[0]);printType(op, varType(op->data[0]));print("*)var_%lld;", op->data[1]);
                break;
            }
            case OP_QUERY_CLASS:
            {
                print("var_%lld=*(", op->data[0]);printType(op, varType(op->data[0]));print("*)((char *)var_%lld+%lld);", op->data[1], op->data[2]);
                break;
            }
            case OP_QUERY_PIPE:
            {
                logError(ir->filename, ir->code, op->code_start, op->code_end, "pipes are unsupported in gpu provider");
                break;
            }
             
            case OP_BOR:   print("var_%lld=var_%lld|var_%lld;", op->data[0], op->data[1], op->data[2]); break;
            case OP_BAND:  print("var_%lld=var_%lld&var_%lld;", op->data[0], op->data[1], op->data[2]); break;
            case OP_BXOR:  print("var_%lld=var_%lld^var_%lld;", op->data[0], op->data[1], op->data[2]); break;
            case OP_SHL:   print("var_%lld=var_%lld<<var_%lld;", op->data[0], op->data[1], op->data[2]); break;
            case OP_SHR:   print("var_%lld=var_%lld>>var_%lld;", op->data[0], op->data[1], op->data[2]); break;
            case OP_BNOT:  print("var_%lld=~var_%lld;", op->data[0], op->data[1]); break;
            case OP_ADD:   print("var_%lld=var_%lld+var_%lld;", op->data[0], op->data[1], op->data[2]); break;
            case OP_SUB:   print("var_%lld=var_%lld-var_%lld;", op->data[0], op->data[1], op->data[2]); break;
            case OP_MUL:   print("var_%lld=var_%lld*var_%lld;", op->data[0], op->data[1], op->data[2]); break;
            case OP_DIV:   print("var_%lld=var_%lld/var_%lld;", op->data[0], op->data[1], op->data[2]); break;
            case OP_MOD:   print("var_%lld=var_%lld%%var_%lld;", op->data[0], op->data[1], op->data[2]); break;
            case OP_EQ:    print("var_%lld=(var_%lld==var_%lld?-1:0);", op->data[0], op->data[1], op->data[2]); break;
            case OP_NE:    print("var_%lld=(var_%lld!=var_%lld?-1:0);", op->data[0], op->data[1], op->data[2]); break;
            case OP_LT:    print("var_%lld=(var_%lld<var_%lld?-1:0);", op->data[0], op->data[1], op->data[2]); break;
            case OP_LE:    print("var_%lld=(var_%lld<=var_%lld?-1:0);", op->data[0], op->data[1], op->data[2]); break;
            case OP_GT:    print("var_%lld=(var_%lld>var_%lld?-1:0);", op->data[0], op->data[1], op->data[2]); break;
            case OP_GE:    print("var_%lld=(var_%lld>=var_%lld?-1:0);", op->data[0], op->data[1], op->data[2]); break;
        }

        for (auto &n : views::reverse(op->next))
        {
            toBuild.push_back({n, op});
        }
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

    pair<BYTE *, BYTE *> ExportToFile(BYTE *header, BYTE *body, int64_t bodyOffset)
    {        
        (void)bodyOffset;
        /* add gpu workers positions */
        {
            *header++ = GetHeaderId(HEADER_GPU_WORKERS);
            *(uint64_t *)header = resultWorkerPositions.size();
            header += 8;
            for (auto &[id, pos] : resultWorkerPositions)
            {
                printf("Export worker %lld with offset %016llx\n", GetExportWorkerId(ir, id, "gpu"), pos.first + bodyOffset);
                /* export id */
                *(uint64_t *)header = GetExportWorkerId(ir, id, "gpu");
                header += 8;
                /* export position */
                *(uint64_t *)header = pos.first + bodyOffset;
                header += 8;
                /* export end */
                *(uint64_t *)header = pos.second + bodyOffset;
                header += 8;
                /* export input table size */
                *(uint64_t *)header = GetWorkerInputTableSize(id);
                header += 8;
                /* export config vars count */
                *(uint64_t *)header = resultConfigVariables[id].size();
                header += 8;
                for (auto [elsize, access] : resultConfigVariables[id])
                {
                    *(uint64_t *)header = elsize;
                    header += 8;
                    *header++ = access;
                }
            }
        }
        memcpy(body, assemblyCode, assemblyEnd - assemblyCode);
        body += assemblyEnd - assemblyCode;
        return {header, body};
    }
};


CodeAssembler *new_gpu_Assembler()
{
    return new WinGPUAssembler();
}
