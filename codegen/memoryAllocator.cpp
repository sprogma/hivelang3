#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <math.h>
#include <vector>
#include <bitset>
#include <array>
#include <map>

using namespace std;


#include "../ir.hpp"
#include "../optimization/optimizer.hpp"
#include "codegen.hpp"
#include "registerAllocator.hpp"
#include "memoryAllocator.hpp"

class SpreadMemory : public ISpreadMemory
{
    WorkerDeclarationContext *current;
public:
    pair<map<int64_t, int64_t>, int64_t> spreadMemory(WorkerDeclarationContext *wk) override 
    {
        current = wk;

        // find memory variables
        memoryVars.clear();
        for (auto &op : wk->content->code)
        {
            if (op->type == OP_LOAD || op->type == OP_STORE)
            {
                memoryVars.insert(op->data[1]);
            }
        }
        

        // select 5-6 random orders, and try to allocate memory for variables.
        // select best one
        vector<int64_t> order;
        for (auto &k : memoryVars)
        {
            order.push_back(k);
        }

        vector<int64_t> best = order;
        int64_t usedMem = getMemory(order);
        // for (int64_t i = 0; i < 1000; ++i)
        for (int64_t i = 0; i < 1; ++i)
        {
            for (int64_t j = 1; j < (int64_t)order.size(); ++j)
            {
                swap(order[rand() % (j + 1)], order[j]);
            }
            int64_t newMem = getMemory(order);
            if (newMem < usedMem)
            {
                best = order;
                usedMem = newMem;
            }
        }

        /* allocate using type */
        map<int64_t, int64_t> res = getMemoryMap(best);
        return {res, usedMem};
    }
    
private:
    set<int64_t> memoryVars;
    vector<set<int64_t>> used;
    
    int64_t size(int64_t var)
    {
        return current->content->variables[var]->size;
    }

    void PrepareAllocate()
    {
        used.clear();
    }
    
    int64_t Allocate(int64_t var)
    {
        int64_t varSize = size(var);
        /* find segment of given size */
        int64_t lastBad = -1;
        int64_t i = 0;
        while (1)
        {
            if (i >= (int64_t)used.size()) { used.resize(i + 1); }
            for (auto &j : used[i])
            {
                (void)j;
                if (true) // TODO: better analys of overlapped memory segments
                {
                    lastBad = i;
                    break;
                }
            }
            // if fit space and right align
            if (i - lastBad >= varSize && (i - varSize + 1) % varSize == 0)
            {
                /* fill this memory and return it */
                for (int64_t j = i - varSize + 1; j <= i; ++j)
                {
                    used[j].insert(var);
                }
                return i - varSize + 1;
            }
            ++i;
        }
    }

    int64_t getMemory(const vector<int64_t> &order)
    {        
        PrepareAllocate();
        
        for (auto &var : order)
        {
            Allocate(var);
        }
        
        return used.size();
    }

    map<int64_t, int64_t> getMemoryMap(const vector<int64_t> &order)
    {
        PrepareAllocate();
        
        map<int64_t, int64_t> res;
        
        for (auto &var : order)
        {
            res[var] = Allocate(var);
        }
        
        return res;
    }
};


ISpreadMemory *newSpreadMemory()
{
    return new SpreadMemory();
}
