#include "optimizer.hpp"

#include <vector>
#include <map>

#include "../ir.hpp"



BuildResult *forkResult(BuildResult *input)
{
    BuildResult *res = new BuildResult;
    for (auto &wk : input->workers)
    {
        res->workers.push_back(new WorkerDeclarationContext(
            wk->name,
            wk->attributes,
            wk->inputs,
            wk->outputs,
            new WorkerContext(
                wk->content->code,
                wk->content->variables
            )
        ));
    }
    return res;
}
