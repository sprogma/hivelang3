#ifndef REGISTER_ALLOCATOR_HPP
#define REGISTER_ALLOCATOR_HPP


#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <math.h>
#include <vector>
#include <bitset>
#include <ranges>
#include <array>
#include <map>

using namespace std;


#include "../ir.hpp"
#include "../optimization/optimizer.hpp"
#include "../analysis/analizators.hpp"
#include "codegen.hpp"
#include "registerAllocator.hpp"


template<typename T, auto A>
int64_t FindFirst(array<T, A> &arr)
{
    for (int64_t i = 0; i < (int64_t)A; ++i)
    {
        if (arr[i].empty()) return i;
    }
    return -1;
}


template<int64_t registersCount>
class SpreadRegisters
{
    static_assert(registersCount >= 2);
public:
    SpreadRegisters() { }
    ~SpreadRegisters() { }

private:
    WorkerDeclarationContext *wk;
    
    set<int64_t> generatedTemps;

    void getAjancedNodes(int64_t x, set<int64_t> &ajd, map<int64_t, int64_t> &res, RegisterAnalizator &analyser)
    {   
        ajd.clear();
        vector<int64_t> stack;
        stack.push_back(x);
        while (!stack.empty())
        {
            int64_t v = stack.back();
            stack.pop_back();
            if (res[v] != -1)
            {
                ajd.insert(v);
                for (auto &n : analyser.overlaps[v])
                {
                    if (res[n] != -1 && ajd.find(n) == ajd.end())
                    {
                        stack.push_back(n);
                    }
                }
            }
        }
    }

    bool isAjansed(int64_t a, int64_t b, map<int64_t, int64_t> &res, RegisterAnalizator &analyser)
    {
        set<int64_t> ajd;
        getAjancedNodes(a, ajd, res, analyser);
        return ajd.find(b) == ajd.end();
    }

    void swapColor(int64_t x, int64_t oldColor, int64_t newColor, map<int64_t, int64_t> &res)
    {
        set<int64_t> ajd;
        getAjancedNodes(x, ajd, res);
        /* find unused colors */
        for (auto &i : ajd)
        {
            if (res[i] == oldColor) { res[i] = newColor; }
            else if (res[i] == newColor) { res[i] = oldColor; }
        }
    }

    void TryColorSame(int64_t rw, int64_t v, map<int64_t, int64_t> &res, RegisterAnalizator &analyser)
    {
        // else - try to allocate same register
        if (res[rw] != -1)
        {
            // try to color V into color of rw
            array<vector<int64_t>, registersCount> color;
            for (auto &i : analyser.overlaps[v])
            {
                if (res[i] == -1) continue;
                color[res[i]].push_back(i);
            }
            if (color[res[rw]].empty())
            {
                res[v] = res[rw];
            }
        }
        else if (res[v] != -1)
        {
            // try to color RW into color of v
            array<vector<int64_t>, registersCount> color;
            for (auto &i : analyser.overlaps[rw])
            {
                if (res[i] == -1) continue;
                color[res[i]].push_back(i);
            }
            if (color[res[v]].empty())
            {
                res[rw] = res[v];
            }
        }
        else
        {
            // try to color both in same color
            array<vector<int64_t>, registersCount> color;
            // fill using both rw and v
            for (auto &i : analyser.overlaps[rw])
            {
                if (res[i] == -1) continue;
                color[res[i]].push_back(i);
            }
            for (auto &i : analyser.overlaps[v])
            {
                if (res[i] == -1) continue;
                color[res[i]].push_back(i);
            }
            // if ok - color both nodes
            // color to -1 if can't find
            res[rw] = res[v] = FindFirst(color);
        }
    }

    void trySpeadSameRegisters(vector<int64_t> order, map<int64_t, int64_t> &res, RegisterAnalizator &analyser)
    {
        /* first pass: if X=Y*Z; free Y -> allocate for X same as for Y */
        for (auto &v : order)
        {
            for (auto &rw : analyser.readWriteOverlapsFirstOperand[v])
            {
                // if them overlaps in other place - don't create same color
                if (analyser.overlaps[v].contains(rw))
                {
                    continue;
                }
                TryColorSame(rw, v, res, analyser);
            }
        } 
        /* simple euristic - if operator writes to X from Y and frees Y - allocate for X the same register */
        for (auto &v : order)
        {
            for (auto &rw : analyser.readWriteOverlaps[v])
            {
                // if them overlaps in other place - don't create same color
                if (analyser.overlaps[v].contains(rw))
                {
                    continue;
                }
                TryColorSame(rw, v, res, analyser);
            }
        } 
    }

    int64_t trySpreadRegisters(map<int64_t, int64_t> &res, RegisterAnalizator &analyser)
    {
        /* color */
        set<pair<int64_t, int64_t>> order; // prior, value
        res.clear();
        for (auto &[k, v] : analyser.overlaps)
        {
            res[k] = -1;
            order.insert({-v.size(), k});
        }

        vector<int64_t> sameOrder(views::keys(res).begin(), views::keys(res).end());
        double bestColored = 0;
        for (int64_t t = 0; t < 1000; ++t)
        {
            for (int64_t i = 1; i < (int64_t)sameOrder.size(); ++i)
            {
                swap(sameOrder[i], sameOrder[rand() % (i + 1)]);
            }
            
            map<int64_t, int64_t> tmpRes;
            
            for (auto &[k, v] : res) tmpRes[k] = -1;
            
            trySpeadSameRegisters(sameOrder, tmpRes, analyser);
            
            double curColored = 0;
            for (auto &v : sameOrder)
            {
                curColored += 0.2 * (tmpRes[v] != -1);
                for (auto &rw : analyser.readWriteOverlaps[v])
                {
                    // count same colored pairs
                    curColored += (tmpRes[v] == tmpRes[rw] && tmpRes[v] != -1);
                }
                for (auto &rw : analyser.readWriteOverlapsFirstOperand[v])
                {
                    // count same colored pairs
                    curColored += 2.0 * (tmpRes[v] == tmpRes[rw] && tmpRes[v] != -1);
                }
            }
            
            if (curColored > bestColored)
            {
                res = tmpRes;
                bestColored = curColored;
            }
        }
        printf("Colored same colors: %f\n", bestColored);
        
        for (auto &[cost, v] : order)
        {
            if (res[v] != -1) continue;
            
            array<vector<int64_t>, registersCount> color;
            
            for (auto &i : analyser.overlaps[v])
            {
                if (res[i] == -1) continue;
                color[res[i]].push_back(i);
            }
            
            int64_t freeColor = FindFirst(color);
            
            if (freeColor == -1)
            {
                // TODO: is there some ways to fix color?
                
                /* Error - can't resolve conflict */
                // TODO: may be select another variable to split?
                /* return it */
                double bestValue = 0.0;
                int64_t to_split = -1;
                for (auto &[var, neibours] : analyser.overlaps)
                {
                    double thisValue = 0.0;
                    // simple euristics:

                    
                    // * if variable have long lifetime - it is good
                    thisValue += analyser.lifetime[var];
                    // * if variable have many ajansed variables - it is good too.
                    thisValue += neibours.size();
                    // * if variable is used often - it is bad
                    thisValue -= 4.2 * pow(analyser.usages[var], 1.22);

                    // printf("%lld value=%f\n", var, thisValue);
                    
                    if ((to_split == -1 || thisValue > bestValue) && !generatedTemps.contains(var))
                    {
                        bestValue = thisValue;
                        to_split = var;
                    }
                }
                if (to_split == -1)
                {
                    printf("Error: there is no variables, but graph coloring was failed???\n");
                    exit(1);
                }
                return to_split;
            }
            res[v] = freeColor;
        }
        return -1;
    }


    // map variable -> registerId
public:
    map<int64_t, int64_t> spreadRegisters(WorkerDeclarationContext *_wk)
    {
        wk = _wk;
        map<int64_t, int64_t> res;
        
        // map<OperationBlock *, int64_t> pressure; // register pressing euristic
        // calculatePressure(pressure);

        generatedTemps.clear();
        int64_t maxOptimizableIterations = 1000;

        /* try to spread registers */
        int64_t iter = 0;
        while (1)
        {
            iter++;
            printf("trying to color %lld...\n", iter);
            RegisterAnalizator analyser(wk);
            int64_t to_split = trySpreadRegisters(res, analyser);
            if (to_split == -1)
            {
                printf("Success!\n");
                for (auto &[k, v] : res)
                {
                    printf("Variable %lld have color %lld\n", k, v);
                }
                return res;
            }
            printf("failed. Spliting %lld...\n", to_split);
            // TODO: implement clever not full split algo
            if (iter > maxOptimizableIterations || true)
            {
                /* simply unload/load variable in all occurences */
                for (int64_t i = 0; i < (int64_t)wk->content->code.size(); ++i)
                {
                    OperationBlock *op = wk->content->code[i];
                    
                    const auto &usedVars = getUsedVariables(op);
                    bool found = false;
                    for (auto &var : usedVars) { if (var == to_split) { found = true; break; } }
                    if (found)
                    {
                        bool foundRead = false;
                        bool foundWrite = false;
                        const auto &readVars = getReadVariables(op);
                        for (auto &var : readVars) { if (var == to_split) { foundRead = true; break; } }    
                        const auto &writtenVars = getWritedVariables(op);
                        for (auto &var : writtenVars) { if (var == to_split) { foundWrite = true; break; } }    
                        
                        int64_t temp = newTemp(wk, wk->content->variables[to_split]);
                        generatedTemps.insert(temp);
                        
                        printf("generated temporary variable: %lld [%d %d]\n", temp, foundRead, foundWrite);
                        
                        if (foundRead)
                        {
                            connectBeforeOp(wk, op, new OperationBlock(OP_LOAD, {temp, to_split}));
                        }
                        connectOp(wk, op, new OperationBlock(OP_FREE_TEMP, {temp}));
                        /* if write - add store too */
                        if (foundWrite)
                        {
                            connectOp(wk, op, new OperationBlock(OP_STORE, {temp, to_split}));
                        }
                        /* translate var id */
                        applyNamesTranslition(op, map<int64_t, int64_t>{{to_split, temp}});
                    }
                }
            }
            else
            {
                /* clever split of variable: select most pressure using point, and insert load/store */
            }
        }
    }
};



#endif
