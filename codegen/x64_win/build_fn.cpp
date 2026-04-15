#include "codegen.hpp"


void WinX64Assembler::BuildFn(WorkerDeclarationContext *wk, int64_t workerId)
{
    if (wk->content == NULL)
    {
        logError(ir->filename, ir->code, wk->code_start, wk->code_end, "x64 doesn't supports workers without body");
        return;
    }
    
    current = wk;

    // update call instructions
    ExpandCallInstructions(wk);

    // create registers translation
    {
        SpreadRegisters<registersCount> regSprd;
        regTable = regSprd.spreadRegisters(wk);
    }

    // allocate memory
    {
        ISpreadMemory *memSprd = newSpreadMemory();
        const auto &[tbl, sz] = memSprd->spreadMemory(wk);
        memTable = tbl;
        usedMemory = sz;
        for (auto &[k, v] : memTable)
        {
            v += 8;
        }
    }

    // print memory table
    for (auto &[k, v] : memTable)
    {
        printf("Var %lld have offset %lld\n", k, v);
    }
    printf("Total size of variables: %lld\n", usedMemory);

    dumpIR(wk);

    // analyze code
    analyzer = new RegisterAnalizator(wk);

    // get used labels
    JumpInstructions.clear();
    
    // generate code

    resultWorkerPositions[workerId] = assemblyEnd - assemblyCode;
    
    printf("worker_%lld:\n", workerId);
    if (wk->content == NULL)
    {
        printf("For now, x64-win doesn't support dynamic linking\n");
    }
    else
    {
        addressTable.clear();
        orderTable.clear();

        toBuild.push_back({wk->content->entry, NULL});
        while (!toBuild.empty())
        {
            BuildOperation();
        }
    }

    // join code using JumpInstructions

    resultWorkerSize[workerId] = (assemblyEnd - assemblyCode) - resultWorkerPositions[workerId];
    
    InsertJumpInstructions();

    delete analyzer;
}


