#include "optimizer.hpp"

#include <vector>
#include <map>

#include "../utils.hpp"
#include "../ir.hpp"


class CompressPromiseLayer : public OptimizationLayer
{
public:
    CompressPromiseLayer()
    { }

    void Apply(BuildResult *state) override
    {
        printf("CompressPromise layer ----------\n");
        for (auto &[fn, key] : state->workers)
        {
            if (fn->content == NULL) continue;
            ApplyFn(fn);
        }
    }
    
    ~CompressPromiseLayer() override
    {}
    
private:
    void ApplyFn(WorkerDeclarationContext *fn)
    {
        // TODO: more strict analys
        for (int64_t i = 0; i < (int64_t)fn->content->code.size(); ++i)
        {
            auto op = fn->content->code[i];
            
            if (op->type == OP_NEW_PROMISE)
            {
                int64_t id = op->data[0];
                
                /* test - all occurences of id are in OP_QUERY_PROMISE? */
                bool found = false;
                printf("test %lld...\n", id);
                for (auto &op2 : fn->content->code)
                {
                    if (op != op2 && op2->type != OP_FREE_TEMP)
                    {
                        if (op2->type == OP_QUERY_PROMISE)
                        {
                            // if data[1] == id - it is normal
                            found = op2->data[0] == id;
                        }
                        else if (op2->type == OP_PUSH_PROMISE)
                        {
                            // if data[0] == id - it is normal
                            found = op2->data[1] == id;
                        }
                        else
                        {    
                            for (auto &var : getUsedVariables(op2))
                            {
                                if (var == id) { found = true; break; }
                            }
                        }
                    }
                    if (found) break;
                }

                if (!found)
                {
                    /* compress: */
                    /* change type of variable on it's base type */
                    TypeContext *type = fn->content->variables[id]->_vector.base;
                    fn->content->variables[id] = type;
                    /* replace all OP_QUERY_PROMISE on OP_QUERY_VAR and OP_PUSH_PROMISE on OP_PUSH_VAR */
                    for (int64_t j = 0; j < (int64_t)fn->content->code.size(); ++j)
                    {
                        auto op2 = fn->content->code[j];
                        
                        if (op2->type == OP_QUERY_PROMISE && op2->data[1] == id)
                        {
                            op2->type = OP_MOV;
                            op2->attributes.clear();
                        }
                        if (op2->type == OP_PUSH_PROMISE && op2->data[0] == id)
                        {
                            op2->type = OP_MOV;
                            op2->attributes.clear();
                        }
                        if (op2->type == OP_NEW_PROMISE)
                        {
                            // remove instruction
                            removeOp(fn, op2);
                            if (i >= j) { --i; }
                            --j;
                        }
                    }
                    
                    printf("Compress promise into %s [id=%lld]\n", fn->name.c_str(), id);
                }
            }
        }
    }
};


OptimizationLayer *newCompressPromiseLayer()
{
    CompressPromiseLayer *result = new CompressPromiseLayer();
    return result;
}

