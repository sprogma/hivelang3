#include "optimizer.hpp"

#include <vector>
#include <map>

#include "../utils.hpp"
#include "../ir.hpp"


class InlineLayer : public OptimizationLayer
{
public:
    double agression;

    InlineLayer(double agression) : agression(agression)
    { }

    void Apply(BuildResult *state) override
    {
        map<WorkerDeclarationContext *, double> cost;
        map<WorkerDeclarationContext *, int64_t> calls;
        map<int64_t, WorkerDeclarationContext *> workerId;

        printf("Inline layer ----------\n");
        
        for (auto &[fn, key] : state->workers)
        {
            if (fn->content == NULL) continue;
            workerId[key] = fn;
            
            double value = 0.0;
            for (auto &op : fn->content->code)
            {
                if (op->type == OP_CALL || IS_JUMP(op->type))
                {
                    value += 10.0;
                }
                else if (is_one<OP_LOAD_INPUT, OP_LOAD_OUTPUT, OP_FREE_TEMP>(op->type))
                {
                    value += 0.1;
                }
                else
                {
                    value += 1.0;
                }
            }
            cost[fn] = value;
            printf("cost of %s = %f\n", fn->name.c_str(), value);
        }

        /* count calls */
        for (auto &[fn, key] : state->workers)
        {
            if (fn->content == NULL) continue;
            for (auto &op : fn->content->code)
            {
                if (op->type == OP_CALL)
                {
                    calls[workerId[op->data[0]]]++;
                }
            }
        }
        
        printf("Apply inlinings...\n");

        /* apply inlinings */
        for (auto &[fn, key] : state->workers)
        {
            if (fn->content == NULL) continue;
            for (int64_t i = 0; i < (int64_t)fn->content->code.size(); ++i)
            {
                if (fn->content->code[i]->type != OP_CALL) 
                { continue; }
                
                auto wk = workerId[fn->content->code[i]->data[0]];

                bool was_allocated = false;
                if (wk == fn)
                {
                    wk = forkWorker(fn);
                    cost[wk] = cost[fn];
                    was_allocated = true;
                }
                
                if (wk->content == NULL) continue; // external function can't be inlined
                
                bool optimize = cost[wk] < agression || calls[wk] == 1;
                
                printf("Insert call of %s into %s? [%f < %f || %lld == 1]\n", wk->name.c_str(), fn->name.c_str(), cost[wk], agression, calls[wk]);
                
                if (optimize)
                {
                    calls[wk]--;
                    cost[fn] += cost[wk];
                    
                    printf("Insert call of %s into %s...\n", wk->name.c_str(), fn->name.c_str());
                    /* allocate temp for all variables in wk */
                    map<int64_t, int64_t> mapTable;
                    for (auto &[id, type] : wk->content->variables)
                    {
                        printf("temp [%lld] [%p vs %p]...\n", id, fn->content, wk->content);
                        mapTable[id] = newTemp(fn, type);
                        printf("ok\n");
                    }
                    printf("Temps generated...\n");
                    for (auto &[k, v] : mapTable)
                    {
                        connectOp(fn, fn->content->code[i], new OperationBlock(OP_FREE_TEMP, {v}));
                    }
                    OperationBlock *next = fn->content->code[i]->next[0];
                    OperationBlock *entry = InsertFunction(fn, mapTable, wk->content->entry, next);
                    printf("Connecting...\n");
                    if (fn->content->code[i]->next[0])
                    {
                        fn->content->code[i]->next[0]->prev.erase(fn->content->code[i]);
                    }
                    fn->content->code[i]->next = {entry};
                    entry->prev.insert(fn->content->code[i]);
                    printf("Inserted\n");
                    removeOp(fn, fn->content->code[i]);

                    dumpIR(fn);
                }

                if (was_allocated)
                {
                    delete wk;
                }
            }
        }
    }

    ~InlineLayer() override
    {}

private:
    map<OperationBlock *, OperationBlock *> codeMap;
    
    OperationBlock *InsertFunction(WorkerDeclarationContext* fn, map<int64_t, int64_t> mapTable, OperationBlock *copy, OperationBlock *next)
    {
        codeMap.clear();
        return InsertFunctionR(fn, mapTable, copy, next);
    }
    
    OperationBlock *InsertFunctionR(WorkerDeclarationContext* fn, map<int64_t, int64_t> mapTable, OperationBlock *copy, OperationBlock *next)
    {
        printf("MAP %p\n", copy);
        if (codeMap.find(copy) == codeMap.end())
        {
            if (copy == NULL) { codeMap[copy] = next; }
            else
            {
                OperationBlock *res = new OperationBlock(copy->type, copy->data, copy->attributes);
                fn->content->code.push_back(res);
                
                for (auto &n : copy->next)
                {
                    res->next.push_back(InsertFunctionR(fn, mapTable, n, next));
                    if (res->next.back())
                    {
                        res->next.back()->prev.insert(res); 
                    }
                }
                
                /* apply map table */

                codeMap[copy] = res;
            }
        }
        return codeMap[copy];
    }
};


OptimizationLayer *newInlineLayer(double agression)
{
    InlineLayer *result = new InlineLayer(agression);
    return result;
}
