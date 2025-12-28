#ifndef OPTIMIZER_HPP
#define OPTIMIZER_HPP

#include <vector>
#include <map>

using namespace std;

#include "../ir.hpp"

class OptimizationLayer
{
public:
    virtual void Apply(BuildResult *state)
    {
        (void)state;
    }
    
    virtual vector<BuildResult *> Apply(vector<BuildResult *>state)
    {
        for (auto &st : state)
        {
            Apply(st);
        }
        return state;
    }
    virtual ~OptimizationLayer() {}
};

class Optimizer
{
    vector<OptimizationLayer *> layers;

public:
    void AddLayer(OptimizationLayer *layer)
    {
        layers.push_back(layer);
    }

    BuildResult *Apply(BuildResult *input)
    {
        vector<BuildResult *> variants {input};

        /* for now, simply apply each layer */
        
        for (auto lay : layers)
        {
            variants = lay->Apply(variants);
        }
        
        return variants[0];
    }

    ~Optimizer()
    {
        for (auto lay : layers)
        {
            delete lay;
        }
    }    
};

BuildResult *forkResult(BuildResult *input);
WorkerDeclarationContext *forkWorker(WorkerDeclarationContext *input);

void freeTemp(WorkerDeclarationContext *wk, OperationBlock *code, int64_t id);
int64_t newTemp(WorkerDeclarationContext *wk, TypeContext *type);
void removeOp(WorkerDeclarationContext *wk, OperationBlock *code);
void connectOp(WorkerDeclarationContext *wk, OperationBlock *code, OperationBlock *next);

/* known layers */

OptimizationLayer *newInlineLayer(double agression);

#endif
