#include "optimizer.hpp"

#include <vector>
#include <map>

#include "../ir.hpp"
#include "optimizer.hpp"


BuildResult *forkResult(BuildResult *input)
{
    BuildResult *res = new BuildResult;
    res->cost = input->cost;
    for (auto &[wk, key] : input->workers)
    {
        res->workers[forkWorker(wk)] = res->nextWorkerId++;
    }
    return res;
}

WorkerDeclarationContext *forkWorker(WorkerDeclarationContext *wk)
{
    vector<OperationBlock *> ops(wk->content->code.size());
    map<OperationBlock *, OperationBlock *> mapTable;
    for (int64_t i = 0; i < (int64_t)wk->content->code.size(); ++i)
    {
        ops[i] = new OperationBlock(wk->content->code[i]->type,
                                    wk->content->code[i]->data,
                                    wk->content->code[i]->attributes);
        mapTable[wk->content->code[i]] = ops[i];
    }
    for (int64_t i = 0; i < (int64_t)wk->content->code.size(); ++i)
    {
        for (auto &next : wk->content->code[i]->next)
        {
            ops[i]->next.push_back(mapTable[next]);
        }
    }
    return new WorkerDeclarationContext(
        wk->name,
        wk->attributes,
        wk->inputs,
        wk->outputs,
        new WorkerContext(
            mapTable[wk->content->entry],
            ops,
            wk->content->variables
        )
    );
}


int64_t newTemp(WorkerDeclarationContext *wk, TypeContext *type)
{
    wk->content->variables[wk->content->nextTempId] = type;
    return wk->content->nextTempId++;
}

void connectOp(WorkerDeclarationContext *wk, OperationBlock *code, OperationBlock *next)
{
    wk->content->code.push_back(next);
    if (code == NULL)
    {
        wk->content->entry = next;
    }
    else
    {
        next->next.insert(next->next.begin(), code->next[0]);
        next->prev.insert(code);
        if (code->next[0])
        {
            code->next[0]->prev.erase(code);
            code->next[0]->prev.insert(next);
        }
        code->next[0] = next;
    }
}

void removeOp(WorkerDeclarationContext *wk, OperationBlock *code)
{
    if (code == NULL) { return; }
    for (auto &p : code->prev)
    {   
        for (auto &nx : p->next)
        {
            if (nx == code)
            {
                nx = code->next[0];
            }
        }
        if (code->next[0])
        {
            code->next[0]->prev.insert(p);
        }
    }
    for (auto &nt : code->next)
    {
        if (nt)
        {
            nt->prev.erase(code);
        }
    }
    if (code == wk->content->entry) 
    {
        wk->content->entry = code->next[0];
    }
    for (int64_t i = 0; i < (int64_t)wk->content->code.size(); ++i)
    {
        if (wk->content->code[i] == code)
        {
            wk->content->code.erase(wk->content->code.begin() + i);
            --i;
        }
    }
}

void freeTemp(WorkerDeclarationContext *wk, OperationBlock *code, int64_t id)
{
    connectOp(wk, code, new OperationBlock(OP_FREE_TEMP, {id}));
}
