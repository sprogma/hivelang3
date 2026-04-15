#include "codegen.hpp"

void WinX64Assembler::ExpandCallInstructions(WorkerDeclarationContext *wk)
{
    for (int64_t i = 0; i < (int64_t)wk->content->code.size(); ++i)
    {
        auto op = wk->content->code[i];
        if (op->type == OP_CALL)
        {
            WorkerDeclarationContext *fn = idToWorker[op->data[0]];
            // insert before it input table generation
            int64_t id = 0, offset = 0, inputTableSize = GetWorkerInputTableSize(op->data[0]);
            for (auto &[name, type] : array{views::all(fn->inputs), 
                                            views::all(fn->outputs)} | views::join)
            {
                int64_t tmp = op->data[id + 1];
                if (id < (int64_t)fn->inputs.size() && wk->content->variables[op->data[id + 1]] != fn->inputs[id].second)
                {
                    tmp = newTemp(wk, fn->inputs[id].second);
                    auto *castOp = new OperationBlock(OP_CAST, {tmp, op->data[id + 1]}, {}, {}, {}, op->code_start, op->code_end);
                    connectBeforeOp(wk, op, castOp);
                }
                
                auto *newOp = new OperationBlock(OP_STORE_INPUT, {tmp, offset - inputTableSize}, {}, {}, {}, op->code_start, op->code_end);
                connectBeforeOp(wk, op, newOp);
                
                if (tmp != op->data[id + 1])
                {
                    auto *freeOp = new OperationBlock(OP_FREE_TEMP, {tmp}, {}, {}, {}, op->code_start, op->code_end);
                    connectOp(wk, op, freeOp);
                }
                
                offset += type->size;
                id++;
            }
            
            // now it have only 1st data
            op->data.resize(1);
        }
    }
}



void WinX64Assembler::InsertJumpInstructions()
{        
    // need to select JumpInstructions sizes
    vector<int64_t> shortJmp(JumpInstructions.size(), 0); // use everythere shortest form
    vector<BYTE *> currentPosition(JumpInstructions.size());
    map<OperationBlock *, BYTE *> opPosition;
    map<pair<BYTE *, int64_t>, int64_t> offsets;
    int64_t totalAddSize = 0;


    stable_sort(JumpInstructions.begin(), JumpInstructions.end(), [](const jmpInstruction &a, const jmpInstruction &b){
        return a.codePos < b.codePos || (a.codePos == b.codePos && a.codeOrder < b.codeOrder);
    });


    bool need_next;
    do
    {
        // update positions
        {
            offsets[{NULL, 0}] = 0;
            
            int64_t lastOffset = 0;
            int64_t id = 0;

            // fill offsets map
            for (auto &i : JumpInstructions)
            {
                currentPosition[id] = i.codePos + lastOffset;
                lastOffset += JMPsize(i.node, i.jmpType, 0, shortJmp[id]).first;
                offsets[{i.codePos, i.codeOrder}] = lastOffset;
                
                id++;
            }

            // save total size
            totalAddSize = lastOffset;
            
            // fill opcode positions
            opPosition.clear();
            for (auto &[op, addr] : addressTable)
            {
                int64_t opOffset = prev(offsets.upper_bound({addr, orderTable[op]}))->second;
                opPosition[op] = addr + opOffset;
            }
        }
        // check if there is any too long jumps
        need_next = false;
        {
            int64_t id = 0;
            for (auto &i : JumpInstructions)
            {
                int64_t need_variant = JMPsize(i.node, i.jmpType, opPosition[i.destOp] - currentPosition[id], shortJmp[id]).second;
                if (need_variant != shortJmp[id])
                {
                    shortJmp[id] = need_variant; // try to use next range level
                    need_next = true;
                }
                id++;
            }
        }
    }
    while (need_next);
    

    // all jumps is now of right size - insert them
    {
        // update addrToLine
        for (auto &[k, v] : addrToLine)
        {
            int64_t opOffset1 = prev(offsets.upper_bound({assemblyCode + v.start, 0}))->second;
            v.start += opOffset1;
            int64_t opOffset2 = prev(offsets.upper_bound({assemblyCode + v.end, 0}))->second;
            v.end += opOffset2;
        }
        
        int64_t id = JumpInstructions.size() - 1;
        BYTE *newAssmeblyEnd = assemblyEnd + totalAddSize;
        BYTE *codeDest = assemblyEnd + totalAddSize;
        BYTE *codeSrc = assemblyEnd;
        for (auto &i : views::reverse(JumpInstructions))
        {
            // copy code block
            int64_t blockSize = codeSrc - i.codePos;
            if (blockSize)
            {
                codeDest -= blockSize;
                codeSrc  -= blockSize;
                memmove(codeDest, codeSrc, blockSize);
            }

            printf("inserted %lld to %lld ... [jmp to %lld [+%lld]] [to instruction %p]\n", shortJmp[id], currentPosition[id] - assemblyCode, opPosition[i.destOp] - assemblyCode, opPosition[i.destOp] - currentPosition[id], i.destOp);
            // insert jump instruction
            codeDest -= JMPsize(i.node, i.jmpType, opPosition[i.destOp] - currentPosition[id], shortJmp[id]).first;
            
            assemblyEnd = codeDest;
            
            assert(codeDest - assemblyCode == currentPosition[id] - assemblyCode);
            
            assert(printJMP(i.node, i.jmpType, opPosition[i.destOp] - currentPosition[id], shortJmp[id]) == shortJmp[id]);

            if (i.jmpType == ASM_JMP)
            {
                // it is part of that block
                markAddress(codeDest, assemblyEnd - codeDest, CodeSpan(i.destOp->code_start, i.destOp->code_end));
            }
            else
            {
                // it is part of 'if'
                markAddress(codeDest, assemblyEnd - codeDest, CodeSpan(i.node->code_start, i.node->code_end));
            }
            
            id--;
        }

        // update header
        for (auto &[k, v] : runtimeApiHeader)
        {
            for (auto &p : v)
            {                   
                int64_t opOffset = prev(offsets.upper_bound({assemblyCode + p.position, p.order}))->second;
                p.position += opOffset;
            }
        }

        // update addressTable
        for (auto &[k, v] : addressTable)
        {
            int64_t opOffset = prev(offsets.upper_bound({v, orderTable[k]}))->second;
            v += opOffset;
        }

        // restore end
        assemblyEnd = newAssmeblyEnd;
    }
}


