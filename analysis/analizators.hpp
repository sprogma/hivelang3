#ifndef ANALIZATORS_HPP
#define ANALIZATORS_HPP

#include "inttypes.h"
#include "../ir.hpp"

class RegisterAnalizator
{
    WorkerDeclarationContext *wk;
    
public:
    map<OperationBlock *, double> executions;
    map<int64_t, double> lifetime;
    map<int64_t, double> usages;
    map<OperationBlock *, set<int64_t>> op_vars;
    map<int64_t, set<int64_t>> overlaps;
    set<int64_t> physicVars;
    
    // op -> var -> [can TO var, can FROM var]
    map<OperationBlock *, map<int64_t, pair<bool, bool>>> access;
    
    RegisterAnalizator(WorkerDeclarationContext *wk) : wk(wk) 
    {
        updateExecutions();
        buildMainConflictGraph();
        findPhysicVariables();
    }
    
private:
    set<OperationBlock *> used;

    void findPhysicVariables()
    {
        physicVars.clear();
        for (auto &op : wk->content->code)
        {
            if (op->type == OP_STORE || op->type == OP_LOAD)
            {
                physicVars.insert(op->data[0]);
            }
        }
    }

    void calculateExecutionWay(OperationBlock *n, double size, map<OperationBlock *, int64_t> &taken)
    {
        if (!n) return;
        executions[n] += size;
        if (IS_JUMP(n->type) && taken[n]++ < 3)
        {
            calculateExecutionWay(n->next[0], size * 0.25, taken);
            calculateExecutionWay(n->next[1], size * 0.75, taken);
            return;
        }
        // if (!used.insert(n).second) return;
        calculateExecutionWay(n->next[0], size, taken);
    }


    void updateExecutions()
    {
        map<OperationBlock *, int64_t> taken;
        used.clear();
        executions.clear();

        calculateExecutionWay(wk->content->entry, 1.0, taken);

        for (auto &[k, v] : executions)
        {
            v = pow(v, 1.5);
            // printf("executions: %p = %f\n", k, v);
        }
    }

    void updateCanFrom(OperationBlock *node, int64_t var)
    {
        if (!node || !used.insert(node).second) return;
        /* can't free from this */
        if (node->type == OP_FREE_TEMP && node->data[0] == var)
        {
            return;
        }
        access[node][var].second = true;
        /* go by forward edges */
        for (auto &n : node->next)
        {
            updateCanFrom(n, var);
        }
    }

    void updateCanTo(OperationBlock *node, int64_t var)
    {
        if (!node || !used.insert(node).second) return;
        /* can't free from this */
        if (node->type == OP_FREE_TEMP && node->data[0] == var)
        {
            return;
        }
        access[node][var].first = true;
        /* go by reverse edges */
        for (auto &n : node->prev)
        {
            updateCanTo(n, var);
        }
    }

    void updateOpVars()
    {
        access.clear();
        usages.clear();
        for (auto op : wk->content->code)
        {
            // getWritedVariables(op) was included into UsedVariables
            for (auto &i : getUsedVariables(op)) 
            { 
                if (op->type != OP_FREE_TEMP) { usages[i] += executions[op]; }
                used.clear(); updateCanTo(op, i);
                used.clear(); updateCanFrom(op, i);
            }
        }
        lifetime.clear();
        op_vars.clear();
        for (auto &[t, resMap] : access)
        {
            for (auto &[k, v] : resMap)
            {
                if (v.first && v.second) 
                { 
                    if (t->type != OP_FREE_TEMP) { lifetime[k] += executions[t]; }
                    op_vars[t].insert(k);
                }
            }
        }
    }

    void buildMainConflictGraph()
    {
        /* analyse live zones */
        updateOpVars();

        /* build graph */
        overlaps.clear();
        // for (auto &op : wk->content->code)
        // {
        //     printf("op %p: ", op);
        //     for (auto &i : op_vars[op])
        //         printf(" %lld", i);
        //     printf("\n");
        // }
        for (auto &[op, vars] : op_vars)
        {
            for (auto &i : vars)
            {
                overlaps[i];
                for (auto &j : vars)
                {
                    if (i != j)
                    {
                        overlaps[i].insert(j);
                    }
                }
            }
        }
        // for (auto &[i, val] : overlaps)
        // {
        //     printf("overlaps: %lld: ", i);
        //     for (auto &j : val)
        //         printf(" %lld", j);
        //     printf("\n");
        // }
    }
};


#endif
