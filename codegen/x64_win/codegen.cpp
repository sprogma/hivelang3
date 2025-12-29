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


template<typename T, auto A>
int64_t FindFirst(array<T, A> &arr)
{
    for (int64_t i = 0; i < (int64_t)A; ++i)
    {
        if (arr[i].empty()) return i;
    }
    return -1;
}


template<int64_t registersCount>
class SpreadRegisters
{
    static_assert(registersCount >= 2);
public:
    SpreadRegisters() { }

private:
    WorkerDeclarationContext *wk;
    
    // id -> block -> [can get TO id, can get FROM id]
    set<OperationBlock *> used;
    set<int64_t> iused;
    map<OperationBlock *, double> executions;
    map<OperationBlock *, set<int64_t>> op_vars;
    map<int64_t, set<OperationBlock *>> vars_op;
    map<OperationBlock *, set<int64_t>> op_using;
    map<int64_t, set<OperationBlock *>> using_op;
    map<int64_t, set<int64_t>> g;

    // op -> var -> [can TO var, can FROM var]
    map<OperationBlock *, map<int64_t, pair<bool, bool>>> access;


    void calculateExecutionWay(OperationBlock *n, OperationBlock *tmp, 
                               map<OperationBlock *, int64_t> &taken,
                               map<OperationBlock *, vector<OperationBlock *>> &executionTmps,
                               map<OperationBlock *, int64_t> &tmpCost)
    {
        if (!n) return;
        executionTmps[n].push_back(tmp);
        if (IS_JUMP(n->type))
        {
            tmpCost[n] = 1;
            if (taken[n]++ < 3)
            {
                calculateExecutionWay(n->next[0], n, taken, executionTmps, tmpCost);
                calculateExecutionWay(n->next[1], n, taken, executionTmps, tmpCost);
                return;
            }
        }
        // if (!used.insert(n).second) return;
        calculateExecutionWay(n->next[0], tmp, taken, executionTmps, tmpCost);
    }


    void updateExecutions()
    {
        map<OperationBlock *, int64_t> taken;
        map<OperationBlock *, vector<OperationBlock *>> executionTmps;
        map<OperationBlock *, int64_t> tmpCost;
        
        used.clear();
        executions.clear();

        tmpCost[NULL] = 1.0;
        calculateExecutionWay(wk->content->entry, NULL, taken, executionTmps, tmpCost);

        for (auto &[k, v] : executionTmps)
        {
            double cost = 0.0;
            for (auto &p : v) { cost += 1.0 / tmpCost[p]; }
            executions[k] = cost;
            printf("executions: %p = %f\n", k, cost);
        }
    }

    void updateCanFrom(OperationBlock *node, int64_t var)
    {
        if (!node || !used.insert(node).second) return;
        /* can't free from this */
        if (node->type == OP_FREE_TEMP && node->data[0] == var)
        {
            return;
        }
        access[node][var].second = true;
        /* go by forward edges */
        for (auto &n : node->next)
        {
            updateCanFrom(n, var);
        }
    }

    void updateCanTo(OperationBlock *node, int64_t var)
    {
        if (!node || !used.insert(node).second) return;
        /* can't free from this */
        if (node->type == OP_FREE_TEMP && node->data[0] == var)
        {
            return;
        }
        access[node][var].first = true;
        /* go by reverse edges */
        for (auto &n : node->prev)
        {
            updateCanTo(n, var);
        }
    }

    void updateOpVars()
    {
        access.clear();
        using_op.clear();
        op_using.clear();
        for (auto op : wk->content->code)
        {
            // getWritedVariables(op) was included into UsedVariables
            for (auto &i : getUsedVariables(op)) 
            { 
                using_op[i].insert(op);
                op_using[op].insert(i);
                used.clear(); updateCanTo(op, i);
                used.clear(); updateCanFrom(op, i);
            }
        }
        op_vars.clear();
        vars_op.clear();
        for (auto &[t, resMap] : access)
        {
            for (auto &[k, v] : resMap)
            {
                if (v.first && v.second) 
                { 
                    op_vars[t].insert(k); 
                    vars_op[k].insert(t); 
                }
            }
        }
    }

    void buildMainConflictGraph()
    {
        /* analyse live zones */
        updateOpVars();

        /* build graph */
        g.clear();
        // for (auto &op : wk->content->code)
        // {
        //     printf("op %p: ", op);
        //     for (auto &i : op_vars[op])
        //         printf(" %lld", i);
        //     printf("\n");
        // }
        for (auto &[op, vars] : op_vars)
        {
            for (auto &i : vars)
            {
                for (auto &j : vars)
                {
                    if (i != j)
                    {
                        g[i].insert(j);
                    }
                }
            }
        }
        // for (auto &[i, val] : g)
        // {
        //     printf("G: %lld: ", i);
        //     for (auto &j : val)
        //         printf(" %lld", j);
        //     printf("\n");
        // }
    }

    void getAjancedNodes(int64_t x, set<int64_t>ajd, map<int64_t, int64_t> &res)
    {   
        ajd.clear();
        vector<int64_t> stack;
        stack.push_back(x);
        while (!stack.empty())
        {
            int64_t v = stack.back();
            stack.pop_back();
            if (res[v] != -1)
            {
                ajd.insert(v);
                for (auto &n : g[v])
                {
                    if (res[n] != -1 && ajd.find(n) == ajd.end())
                    {
                        stack.push_back(n);
                    }
                }
            }
        }
    }

    bool isAjansed(int64_t a, int64_t b, map<int64_t, int64_t> &res)
    {
        set<int64_t> ajd;
        getAjancedNodes(a, ajd, res);
        return ajd.find(b) == ajd.end();
    }

    void swapColor(int64_t x, int64_t oldColor, int64_t newColor, map<int64_t, int64_t> &res)
    {
        set<int64_t> ajd;
        getAjancedNodes(x, ajd, res);
        /* find unused colors */
        for (auto &i : ajd)
        {
            if (res[i] == oldColor) { res[i] = newColor; }
            else if (res[i] == newColor) { res[i] = oldColor; }
        }
    }

    int64_t trySpreadRegisters(map<int64_t, int64_t> &res)
    {
        /* color */
        set<pair<int64_t, int64_t>> order; // prior, value
        res.clear();
        for (auto &[k, v] : g)
        {
            res[k] = -1;
            order.insert({-v.size(), k});
        }
        for (auto &[cost, v] : order)
        {
            array<vector<int64_t>, registersCount> color;
            for (auto &i : g[v])
            {
                if (res[i] == -1) continue;
                color[res[i]].push_back(i);
            }
            int64_t freeColor = FindFirst(color);
            if (freeColor == -1)
            {
                // TODO: is there some ways to fix color?
                
                /* Error - can't resolve conflict */
                // TODO: may be select another variable to split?
                /* return it */
                double bestValue = 0.0;
                int64_t to_split = -1;
                for (auto &[var, neibours] : g)
                {
                    double thisValue = 0.0;
                    // simple euristics:
                    
                    // * if variable have long lifetime - it is good
                    thisValue += vars_op[var].size();
                    // * if variable have many ajansed variables - it is good too.
                    thisValue += neibours.size();
                    // * if variable is used often - it is bad
                    thisValue -= 4.6 * using_op[var].size();

                    printf("%lld value=%f\n", var, thisValue);
                    
                    if (to_split == -1 || thisValue > bestValue)
                    {
                        bestValue = thisValue;
                        to_split = var;
                    }
                }
                if (to_split != -1)
                {
                    printf("Error: there is no variables, but graph coloring was failed???\n");
                }
                return to_split;
            }
            res[v] = freeColor;
        }
        return -1;
    }


    // map variable -> registerId
public:
    map<int64_t, int64_t> spreadRegisters(WorkerDeclarationContext *_wk)
    {
        wk = _wk;
        map<int64_t, int64_t> res;
        
        // map<OperationBlock *, int64_t> pressure; // register pressing euristic
        // calculatePressure(pressure);

        int64_t maxOptimizableIterations = 1000;

        /* try to spread registers */
        int64_t iter = 0;
        while (1)
        {
            iter++;
            printf("trying to color %lld...\n", iter);
            updateExecutions();
            buildMainConflictGraph();
            int64_t to_split = trySpreadRegisters(res);
            if (to_split == -1)
            {
                printf("Success!\n");
                for (auto &[k, v] : res)
                {
                    printf("Variable %lld have color %lld\n", k, v);
                }
                return res;
            }
            printf("failed. Spliting %lld...\n", to_split);
            // TODO: implement clever not full split algo
            if (iter > maxOptimizableIterations || true)
            {
                /* simply unload/load variable in all occurences */
                for (int64_t i = 0; i < (int64_t)wk->content->code.size(); ++i)
                {
                    OperationBlock *op = wk->content->code[i];
                    
                    const auto &usedVars = getUsedVariables(op);
                    bool found = false;
                    for (auto &var : usedVars) { if (var == to_split) { found = true; break; } }
                    if (found)
                    {
                        bool foundRead = true; // TODO: write getReadVariables(op)
                        bool foundWrite = false;
                        const auto &writtenVars = getWritedVariables(op);
                        for (auto &var : writtenVars) { if (var == to_split) { foundWrite = true; break; } }    
                        
                        int64_t temp = newTemp(wk, wk->content->variables[to_split]);
                        printf("generated temporary variable: %lld\n", temp);
                        if (foundRead)
                        {
                            connectBeforeOp(wk, op, new OperationBlock(OP_LOAD, {temp, to_split}));
                        }
                        connectOp(wk, op, new OperationBlock(OP_FREE_TEMP, {temp}));
                        /* if write - add store too */
                        if (foundWrite)
                        {
                            connectOp(wk, op, new OperationBlock(OP_STORE, {temp, to_split}));
                        }
                        /* translate var id */
                        applyNamesTranslition(op, map<int64_t, int64_t>{{to_split, temp}});
                    }
                }
            }
            else
            {
                /* clever split of variable: select most pressure using point, and insert load/store */
            }
        }
    }
};






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
        if (assemblyEnd) *assemblyEnd++ = '\0';
        
        printf("Code generated.\n");
        puts(assemblyCode);
    }

private:
    set<OperationBlock *> generated;
    map<int64_t, pair<int64_t, int64_t>> memoryImage;
    map<int64_t, int64_t> regTable;
    int64_t usedMemory;
    
    static constexpr int64_t registersCount = 5;
    
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

        // prepare build register translation
        SpreadRegisters<registersCount> spr;
        regTable = spr.spreadRegisters(wk);

        dumpIR(wk);

        return;
        
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
                    // auto reg = AcqureVariable(op->data[0], {});
                    // print("\ttest %s\n", RegisterName(reg));
                    print("\tjz op_%p\n", op->next[1]);
                    break;
                }
                case OP_JNZ:
                {
                    // auto reg = AcqureVariable(op->data[0], {});
                    // print("\ttest %s\n", RegisterName(reg));
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
            
                case OP_LOAD: printf("OP_LOAD [not supported]\n"); break;
                case OP_STORE: printf("OP_STORE [not supported]\n"); break;
            
                case OP_LOAD_INPUT: printf("OP_LOAD_INPUT [not supported]\n"); break;
                case OP_LOAD_OUTPUT: printf("OP_LOAD_OUTPUT [not supported]\n"); break;
                
                case OP_FREE_TEMP: printf("OP_FREE_TEMP [not supported]\n"); break;
                
                case OP_CALL: printf("OP_CALL [not supported]\n"); break;
                case OP_CAST: printf("OP_CAST [not supported]\n"); break;
                case OP_MOV: printf("OP_MOV [not supported]\n"); break;

                case OP_NEW_INT: printf("OP_NEW_INT [not supported]\n"); break;
                case OP_NEW_FLOAT: printf("OP_NEW_FLOAT [not supported]\n"); break;
                case OP_NEW_ARRAY: printf("OP_NEW_ARRAY [not supported]\n"); break;
                case OP_NEW_PIPE: printf("OP_NEW_PIPE [not supported]\n"); break;
                case OP_NEW_PROMISE: printf("OP_NEW_PROMISE [not supported]\n"); break;
                case OP_NEW_CLASS: printf("OP_NEW_CLASS [not supported]\n"); break;
                
                case OP_PUSH_VAR: printf("OP_PUSH_VAR [not supported]\n"); break;
                case OP_PUSH_ARRAY: printf("OP_PUSH_ARRAY [not supported]\n"); break;
                case OP_PUSH_PIPE: printf("OP_PUSH_PIPE [not supported]\n"); break;
                case OP_PUSH_PROMISE: printf("OP_PUSH_PROMISE [not supported]\n"); break;
                case OP_PUSH_CLASS: printf("OP_PUSH_CLASS [not supported]\n"); break;
                case OP_QUERY_VAR: printf("OP_QUERY_VAR [not supported]\n"); break;
                case OP_QUERY_ARRAY: printf("OP_QUERY_ARRAY [not supported]\n"); break;
                case OP_QUERY_INDEX: printf("OP_QUERY_INDEX [not supported]\n"); break;
                case OP_QUERY_PIPE: printf("OP_QUERY_PIPE [not supported]\n"); break;
                case OP_QUERY_PROMISE: printf("OP_QUERY_PROMISE [not supported]\n"); break;
                case OP_QUERY_CLASS: printf("OP_QUERY_CLASS [not supported]\n"); break;
                
                case OP_BOR: printf("OP_BOR [not supported]\n"); break;
                case OP_BAND: printf("OP_BAND [not supported]\n"); break;
                case OP_BXOR: printf("OP_BXOR [not supported]\n"); break;
                case OP_SHL: printf("OP_SHL [not supported]\n"); break;
                case OP_SHR: printf("OP_SHR [not supported]\n"); break;
                case OP_BNOT: printf("OP_BNOT [not supported]\n"); break;
                
                case OP_ADD:
                {
                    // auto v0 = AcqureVariable(op->data[0], {}, false);
                    // auto v1 = AcqureVariable(op->data[1], {});
                    // auto v2 = AcqureVariable(op->data[2], {});
                    // print("\txor %s, %s\n", RegisterName({v0.first, 8}), RegisterName({v0.first, 8}));
                    // print("\tadd %s, %s\n", RegisterName({v0.first, 8}), RegisterName({v1.first, 8}));
                    // print("\tadd %s, %s\n", RegisterName({v0.first, 8}), RegisterName({v2.first, 8}));
                    print("\tadd\n");
                    break;
                }
                case OP_SUB: printf("OP_SUB [not supported]\n"); break;
                case OP_MUL: printf("OP_MUL [not supported]\n"); break;
                case OP_DIV: printf("OP_DIV [not supported]\n"); break;
                case OP_MOD: printf("OP_MOD [not supported]\n"); break;
                
                case OP_EQ: printf("OP_EQ [not supported]\n"); break;
                case OP_LT: printf("OP_LT [not supported]\n"); break;
                case OP_LE: printf("OP_LE [not supported]\n"); break;
                case OP_GT: printf("OP_GT [not supported]\n"); break;
                case OP_GE: printf("OP_GE [not supported]\n"); break;
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
