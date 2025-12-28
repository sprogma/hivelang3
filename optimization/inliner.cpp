#include "optimizer.hpp"

#include <vector>
#include <map>

#include "../ir.hpp"


class InlineLayer : OptimizationLayer
{
public:
    double agression;

    InlineLayer(double agression) : agression(agression)
    { }
    
    virtual void Apply(BuildResult *state) 
    {
        (void)state;
        printf("Inliner!\n");
    }

    virtual ~InlineLayer() 
    {}
};
