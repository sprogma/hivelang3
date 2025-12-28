#ifndef OPTIMIZER_HPP
#define OPTIMIZER_HPP

#include <vector>
#include <map>

using namespace std;

#include "../ir.hpp"

class OptimizationLayer
{
public:
    virtual void Apply(BuildResult *state) = 0;
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

    void Apply(BuildResult *input)
    {
        for (auto lay : layers)
        {
            lay->Apply(input);
        }
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


/* known layers */

class InlineLayer : OptimizationLayer
{
    InlineLayer(double agression);
    virtual void Apply(BuildResult *state);
    virtual ~InlineLayer();
};

#endif
