#include "optimizer.hpp"

#include <vector>
#include <map>

#include "../utils.hpp"
#include "../ir.hpp"


class StripUnusedInstructionsLayer : public OptimizationLayer
{
public:
    StripUnusedInstructionsLayer()
    { }

    set<OperationBlock *> used;

    void Apply(BuildResult *state) override
    {
        printf("StripUnusedInstructions layer ----------\n");

        /* update each worker */
        for (auto &[fn, key] : state->workers)
        {
            if (fn->content)
            {
                used.clear();
                GetUsed(fn->content->entry);
                for (int64_t i = 0; i < (int64_t)fn->content->code.size(); ++i)
                {
                    if (!used.contains(fn->content->code[i]))
                    {
                        removeOp(fn, fn->content->code[i]);
                        --i;
                    }
                }
            }
        }
    }

    void GetUsed(OperationBlock *code)
    {
        if (code && used.insert(code).second)
        {
            for (auto &n : code->next)
            {
                GetUsed(n);
            }
        }
    }

    ~StripUnusedInstructionsLayer() override
    {}
};


OptimizationLayer *newStripUnusedInstructionsLayer()
{
    StripUnusedInstructionsLayer *result = new StripUnusedInstructionsLayer();
    return result;
}

