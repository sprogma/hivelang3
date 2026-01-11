#include "optimizer.hpp"

#include <vector>
#include <map>

#include "../utils.hpp"
#include "../ir.hpp"


class StripUnusedFunctionsLayer : public OptimizationLayer
{
public:
    StripUnusedFunctionsLayer()
    { }

    void Apply(BuildResult *state) override
    {
        set<WorkerDeclarationContext *> notCalled;
        map<int64_t, WorkerDeclarationContext *> workerId;

        printf("StripUnused layer ----------\n");
        
        for (auto &[fn, key] : state->workers)
        {
            workerId[key] = fn;
            notCalled.insert(fn);
        }

        /* count is called */        
        for (auto &[fn, key] : state->workers)
        {
            if (fn->attributes.find("export") != fn->attributes.end())
            {
                updateFromWorker(workerId, notCalled, fn);
            }
        }

        for (auto &fn : notCalled)
        {
            printf("not used function %s [! waring, it may be inlined]\n", fn->name.c_str());
            state->workers.erase(fn);
        }
    }

    void updateFromWorker(map<int64_t, WorkerDeclarationContext *> &workerId, 
                          set<WorkerDeclarationContext *> &notCalled, 
                          WorkerDeclarationContext *fn)
    {
        if (notCalled.find(fn) != notCalled.end())
        {
            notCalled.erase(fn);
            
            if (fn->content == NULL) return;
            for (auto &op : fn->content->code)
            {
                if (op->type == OP_CALL)
                {
                    updateFromWorker(workerId, notCalled, workerId[op->data[0]]);
                }
            }
        }
    }

    ~StripUnusedFunctionsLayer() override
    {}
};


OptimizationLayer *newStripUnusedFunctionsLayer()
{
    StripUnusedFunctionsLayer *result = new StripUnusedFunctionsLayer();
    return result;
}

