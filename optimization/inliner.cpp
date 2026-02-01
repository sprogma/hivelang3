#include "optimizer.hpp"

#include <vector>
#include <map>

#include "../codegen/codegen.hpp"
#include "../utils.hpp"
#include "../ir.hpp"


class InlineLayer : public OptimizationLayer
{
public:
    double agression;

    const double one_call_multipler = 10.0;

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
            workerId[key] = fn;
        }

        /* count calls */
        for (auto &[fn, key] : state->workers)
        {
            if (fn->content == NULL) continue;
            for (auto &op : fn->content->code)
            {
                if (op->type == OP_CALL)
                {
                    if (workerId[op->data[0]] == fn)
                    {
                        calls[fn] = 1000; // recursive function - many calls
                        // actually, don't need this, but let it be
                    }
                    calls[workerId[op->data[0]]]++;
                }
            }
        }
        
        for (auto &[fn, key] : state->workers)
        {
            if (fn->content == NULL) continue;
                
            
            double value = 0.0;
            for (auto &op : fn->content->code)
            {
                if (op->type == OP_CALL || IS_JUMP(op->type))
                {
                    value += 10.0;
                }
                else if (is_one<OP_QUERY_ARRAY, OP_QUERY_INDEX, OP_QUERY_PIPE, OP_QUERY_PROMISE, OP_QUERY_CLASS>(op->type))
                {
                    value += 3.5;
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
            printf("cost of %s = %f", fn->name.c_str(), value);
            if (calls[fn] == 1) printf(" / %f = %f", one_call_multipler, value / one_call_multipler);
            printf(", %lld calls\n", calls[fn]);
        }
        
        printf("Apply inlinings...\n");

        /* apply inlinings */
        for (auto &[fn, key] : state->workers)
        {
            if (fn->content == NULL) continue;
            for (int64_t i = 0; i < (int64_t)fn->content->code.size(); ++i)
            {
                OperationBlock *node = fn->content->code[i];
                if (fn->content->code[i]->type != OP_CALL) 
                { continue; }
                
                auto wk = workerId[fn->content->code[i]->data[0]];
                
                if (wk->content == NULL) continue; // external function can't be inlined

                bool was_allocated = false;
                if (wk == fn)
                {
                    wk = forkWorker(fn);
                    cost[wk] = cost[fn];
                    calls[wk] = calls[fn];
                    was_allocated = true;
                }

                bool optimize = cost[wk] < agression || (calls[wk] == 1 && cost[wk] < agression * one_call_multipler);
                
                if (optimize && 
                    !wk->attributes.contains("noinline") && 
                    !node->attributes.contains("noinline") &&
                    AllowInlining(get<string>(node->attributes["provider"])))
                {
                    cost[fn] += cost[wk];
                    
                    printf("Insert call of %s into %s...\n", wk->name.c_str(), fn->name.c_str());
                    /* parse inputs */
                    vector<int64_t> inputs;
                    set<int64_t> outputs;
                    map<int64_t, int64_t> mapTable;
                    for (int64_t id = 0; id < (int64_t)wk->inputs.size(); ++id)
                    {
                        inputs.push_back(fn->content->code[i]->data[1 + id]);
                    }
                    for (int64_t id = 0; id < (int64_t)wk->outputs.size(); ++id)
                    {
                        /* find variable used for this output - don't create temporary for it */
                        int64_t varName = GetOutputVariable(wk, id);
                        mapTable[varName] = fn->content->code[i]->data[1 + wk->inputs.size() + id];
                        outputs.insert(varName);
                    }
                    /* allocate temp for all variables in wk */
                    for (auto &[id, type] : wk->content->variables)
                    {
                        if (outputs.find(id) == outputs.end())
                        {
                            mapTable[id] = newTemp(fn, type);
                        }
                    }
                    // free new temporaries
                    for (auto &[k, v] : mapTable)
                    {
                        // need to free only variables
                        if (k < FIRST_TEMP_ID && outputs.find(k) == outputs.end())
                        {
                            connectOp(fn, fn->content->code[i], new OperationBlock(OP_FREE_TEMP, {v}));
                        }
                    }
                    OperationBlock *next = fn->content->code[i]->next[0];
                    OperationBlock *entry = InsertFunction(fn, inputs, mapTable, wk->content->entry, next);
                    // connect inserted code
                    if (fn->content->code[i]->next[0])
                    {
                        fn->content->code[i]->next[0]->prev.erase(fn->content->code[i]);
                    }
                    fn->content->code[i]->next = {entry};
                    entry->prev.insert(fn->content->code[i]);
                    // remove call
                    removeOp(fn, fn->content->code[i]);
                    --i; /* adjust iterator */
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
    int64_t GetOutputVariable(WorkerDeclarationContext *fn, int64_t id)
    {
        for (auto &op : fn->content->code)
        {
            if (op->type == OP_LOAD_OUTPUT)
            {
                if (op->data[0] == id)
                {
                    return op->data[1];
                }
            }
        }
        return -1;
    }

    map<OperationBlock *, OperationBlock *> codeMap;
    vector<int64_t> fnInputs;
    
    OperationBlock *InsertFunction(WorkerDeclarationContext* fn, vector<int64_t> &inputs, map<int64_t, int64_t> mapTable, OperationBlock *copy, OperationBlock *next)
    {
        fnInputs = inputs;
        codeMap.clear();
        return InsertFunctionR(fn, mapTable, copy, next);
    }
    
    OperationBlock *InsertFunctionR(WorkerDeclarationContext* fn, map<int64_t, int64_t> mapTable, OperationBlock *copy, OperationBlock *next)
    {
        if (codeMap.find(copy) == codeMap.end())
        {
            if (copy == NULL) { codeMap[copy] = next; }
            else
            {
                if (copy->type == OP_LOAD_OUTPUT)
                {
                    /* simply compile all rest code */
                    codeMap[copy] = InsertFunctionR(fn, mapTable, copy->next[0], next);
                }
                else
                {
                    OperationBlock *res = new OperationBlock(copy->type, copy->data, copy->attributes);
                    fn->content->code.push_back(res);
                    codeMap[copy] = res;
                    
                    for (auto &n : copy->next)
                    {
                        res->next.push_back(InsertFunctionR(fn, mapTable, n, next));
                        if (res->next.back())
                        {
                            res->next.back()->prev.insert(res); 
                        }
                    }
                    
                    if (copy->type == OP_LOAD_INPUT)
                    {
                        res->type = OP_MOV;
                        int64_t tmp = res->data[1];
                        res->data[1] = fnInputs[res->data[0]];
                        res->data[0] = mapTable[tmp];
                    }
                    else
                    {
                        /* apply map table */
                        applyNamesTranslition(res, mapTable);
                    }
                }
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
