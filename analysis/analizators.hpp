#ifndef ANALIZATORS_HPP
#define ANALIZATORS_HPP

#include <unordered_set>
#include <unordered_map>
#include "inttypes.h"
#include "../ir.hpp"

class RegisterAnalizator
{
    WorkerDeclarationContext *wk;


    struct pairhash 
    {
    public:
        template <typename T, typename U>
        std::size_t operator()(const std::pair<T, U> &x) const
        {
        return std::hash<T>()(x.first) ^ std::hash<U>()(x.second);
        }
    };
    
public:
    unordered_map<OperationBlock *, double> executions;
    unordered_map<int64_t, double> lifetime;
    unordered_map<int64_t, double> usages;
    unordered_map<OperationBlock *, set<int64_t>> op_vars;
    unordered_map<int64_t, set<int64_t>> overlaps;
    set<int64_t> physicVars;
    // write to key, read from value
    unordered_map<int64_t, set<int64_t>> readWriteOverlaps;
    unordered_map<int64_t, set<int64_t>> readWriteOverlapsFirstOperand;
    
    // op -> var -> [can TO var, can FROM var]
    unordered_map<OperationBlock *, map<int64_t, pair<bool, bool>>> access;
    
    RegisterAnalizator(WorkerDeclarationContext *wk) : wk(wk) 
    {
        updateExecutions();
        buildMainConflictGraph();
        findPhysicVariables();
    }
    
private:
    unordered_set<pair<OperationBlock *, int64_t>, pairhash> used;

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
        if (!node || !used.insert({node, var}).second) return;
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
        if (!node || !used.insert({node, ~var}).second) return;
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
        used.clear();
        for (auto op : wk->content->code)
        {
            // getWritedVariables(op) was included into UsedVariables
            for (auto &i : getUsedVariables(op)) 
            { 
                if (op->type != OP_FREE_TEMP) { usages[i] += executions[op]; }
                updateCanTo(op, i);
                updateCanFrom(op, i);
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
            auto wrt = getWritedVariables(op);
            auto rdv = getReadVariables(op);
            set<int64_t> writedVars(wrt.begin(), wrt.end());
            set<int64_t> readVars(rdv.begin(), rdv.end());
            set<int64_t> freedVars;
            if (op->next.size() == 1)
            {
                OperationBlock *x = op->next[0];
                while (x && x->type == OP_FREE_TEMP)
                {
                    freedVars.insert(x->data[0]);
                    x = x->next[0];
                }
            }
            
            // printf("operator %p\n", op);
            // for (auto &a1 : writedVars) printf("write %lld\n", a1);
            // for (auto &a1 : readVars) printf("read %lld\n", a1);
            // for (auto &a1 : freedVars) printf("free %lld\n", a1);
            
            for (auto &i : vars)
            {
                overlaps[i]; // create all vars
                readWriteOverlaps[i]; // create all vars
                readWriteOverlapsFirstOperand[i]; // create all vars
                for (auto &j : vars)
                {
                    if (i < j) // to add each pair only once
                    {
                        if (((writedVars.contains(i) && readVars.contains(j) && freedVars.contains(j)) ||
                            (writedVars.contains(j) && readVars.contains(i) && freedVars.contains(i))) &&
                            writedVars.size() == 1)
                        {
                            // if we write to I, reading J and after that free J - that means i and j can have one register
                            // [only if writing register is one, to be shure]
                            readWriteOverlaps[i].insert(j);
                            if (rdv.size() >= 1 && j == rdv[0])
                            {
                                readWriteOverlapsFirstOperand[i].insert(j);
                            }
                        }
                        else
                        {
                            overlaps[i].insert(j); overlaps[j].insert(i);
                        }
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
