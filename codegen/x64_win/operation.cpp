#include "codegen.hpp"


// translation table :(


void WinX64Assembler::BuildOperation()
{
    OperationBlock *op = toBuild.back().first;
    OperationBlock *prevOp = toBuild.back().second;
    toBuild.pop_back();
    
    // return from function
    if (op == NULL)
    {
        pbyte(0xC3);
        return;
    }

    int64_t currentOrder = orderTable.size();
    
    // if already this block is compiled - jump to it
    // TODO: here can take some place of "assembly inlining"
    if (!addressTable.insert({op, assemblyEnd}).second) 
    { 
        JumpInstructions.push_back({ASM_JMP, assemblyEnd, currentOrder, op, prevOp});
        return;
    }

    orderTable[op] = currentOrder;

    // if instruction have many next:
    #define ABEL_BINOP(T) \
        if (regTable[op->data[0]] == regTable[op->data[2]]) \
        { \
            printRR(op, T, Register(op->data[2]), Register(op->data[1])); \
        } \
        else \
        { \
            InsertMove(op, op->data[0], op->data[1]); \
            printRR(op, T, Register(op->data[0]), Register(op->data[2])); \
        }
    #define CMPOP(A, B) { \
        printf("%lld %lld %lld [%p %p %p]\n", op->data[0], op->data[1], op->data[2], varType(op->data[0]), varType(op->data[1]), varType(op->data[2])); \
        printRR(op, ASM_CMP, Register(op->data[1]), Register(op->data[2])); \
        printRC(op, ASM_MOV_RC, Register(op->data[0]), 0); \
        auto reg = Register(op->data[0]); \
        reg.second = 1; \
        if (isSigned(op->data[0])) \
            printR(op, A, reg); \
        else \
            printR(op, B, reg); \
        printR(op, ASM_NEG, Register(op->data[0])); \
    }
    
    switch (op->type)
    {
        // impossible
        case OP_JMP: break;    
        // nothing to do
        case OP_FREE_TEMP: break;
        
        case OP_JZ:
        {
            printRR(op, ASM_TEST, Register(op->data[0]), Register(op->data[0]));
            JumpInstructions.push_back({ASM_JZ, assemblyEnd, currentOrder, op->next[1], op});
            break;
        }   
        case OP_JNZ: 
        {
            printRR(op, ASM_TEST, Register(op->data[0]), Register(op->data[0]));
            JumpInstructions.push_back({ASM_JNZ, assemblyEnd, currentOrder, op->next[1], op});
            break;
        }
        case OP_LOAD:
            // mov $0, XX PTR [rbp + $1]
            printRM(op, ASM_MOV_RM, Register(op->data[0]), {5, 8}, memTable[op->data[1]]);
            break;
        case OP_STORE:
            // mov XX PTR [rbp + $1], $0
            printMR(op, ASM_MOV_MR, {5, 8}, Register(op->data[0]), memTable[op->data[1]]);
            break;
    
        case OP_LOAD_INPUT: 
            // mov $0, XX PTR [rdi + $1]
            printRM(op, ASM_MOV_RM, Register(op->data[1]), {7, 8}, GetInputOffset(op->data[0]));
            break;
        case OP_LOAD_OUTPUT: 
            // mov $0, XX PTR [rdi + $1]
            printRM(op, ASM_MOV_RM, Register(op->data[1]), {7, 8}, GetOutputOffset(op->data[0]));
            break;
        
        case OP_STORE_INPUT:
        {
            printMR(op, ASM_MOV_MR, {5, 8}, Register(op->data[0]), op->data[1]);
            break;
        }

        case OP_SLEEP:
        {
            // rdi=time
            InsertMove(op, {7, 8}, Register(op->data[0]), isSigned(op->data[0]));
            runtimeApiHeader[GetHeaderId(ACTION_SLEEP, "x64")].push_back({printCALL(op, 0x0) - assemblyCode, currentOrder});
            break;
        }
        
        case OP_CALL:
        {
            // call table was generated, use it
            int64_t callTableSize = GetWorkerInputTableSize(op->data[0]);
            // rdi="on" parameter
            // rsi=call table
            // rdx=worker id
            InsertInteger(op, {2, 8}, GetExportWorkerId(ir, op->data[0], get<string>(op->attributes["provider"])));
            InsertMove(op, {6, 8}, {5, 8}, false);
            if (op->attributes.contains("on"))
            {
                if (op->attributes["on"] == string("local"))
                {
                    InsertInteger(op, {7, 8}, 1);
                }
                else if (op->attributes["on"] == string("remote"))
                {
                    InsertInteger(op, {7, 8}, 2);
                }
                else if (holds_alternative<int64_t>(op->attributes["on"]))
                {
                    // this is temporary ID that holds global compueter name
                    InsertMove(op, {7, 8}, Register(get<int64_t>(op->attributes["on"])), false);
                }
                else
                {
                    logError(ir->filename, ir->code, op->code_start, op->code_end, "Wrong \"on\" call attribute value: %s", get<string>(op->attributes["on"]).c_str());
                }
            }
            else { InsertInteger(op, {7, 8}, 0); } // default run attribute
            printRC(op, ASM_ADD_RC, {6, 8}, -callTableSize);
            runtimeApiHeader[GetHeaderId(ACTION_CALL_WORKER, get<string>(op->attributes["provider"]))].push_back({printCALL(op, 0x0) - assemblyCode, currentOrder});
            break;
        }
            
        case OP_CAST: 
            // if this is cast scalar->object - all is ok
            // TODO: what to do?
            if ((varType(op->data[0])->type == TYPE_SCALAR) != 
                (varType(op->data[1])->type == TYPE_SCALAR))
            {
                InsertMove(op, op->data[0], op->data[1]);
                break;
            }
            // if provider is same [x64] - move, else - request cast from provider
            if ((varType(op->data[0])->provider == varType(op->data[1])->provider))
            {
                InsertMove(op, op->data[0], op->data[1]);
            }
            else
            {
                if (!ExistsCast(varType(op->data[0])->provider, varType(op->data[1])->provider))
                {
                    logError(ir->filename, ir->code, op->code_start, op->code_end, "Can't cast %s to %s\n", varType(op->data[0])->provider.c_str(), varType(op->data[0])->provider.c_str());
                }
                // rdi=object
                // rsi=toID | (object_type << 8)
                // rdx=fromID | (object_type << 8)
                // rcx=objectSize
                InsertMove(op, {7, 8}, Register(op->data[1]), false);
                InsertInteger(op, {6, 8}, ProviderId(varType(op->data[0])->provider) | (varTypeID(varType(op->data[0])) << 8));
                InsertInteger(op, {2, 8}, ProviderId(varType(op->data[1])->provider) | (varTypeID(varType(op->data[1])) << 8));
                int64_t var_size = 0;
                switch (varType(op->data[1])->type)
                {
                    case TYPE_PROMISE:
                    case TYPE_ARRAY:
                    case TYPE_PIPE:
                        var_size = varType(op->data[1])->_vector.base->size; break;
                    case TYPE_CLASS:
                        var_size = GetClassSize(varType(op->data[1])); break;
                    default:
                }
                InsertInteger(op, {1, 8}, var_size);
                runtimeApiHeader[GetHeaderId(ACTION_CAST_PROVIDER)].push_back({printCALL(op, 0x0) - assemblyCode, currentOrder});
                InsertMove(op, Register(op->data[0]), {7, 8}, false);
                break;
            }
            break;
            
        case OP_MOV: 
            InsertMove(op, op->data[0], op->data[1]);
            break;

        case OP_NEW_INT: 
            InsertInteger(op, Register(op->data[0], 8), op->data[1]);
            break;
            
        case OP_NEW_FLOAT:
            print("\tOP_NEW_FLOAT [not supported]\n"); 
            break;
            
        case OP_NEW_STRING:
        {
            // rdi=OBJECT_DEFINED_ARRAY=5
            // rsi=defined object ID
            // rdx=size of element
            InsertInteger(op, {7, 8}, 0x05);
            InsertInteger(op, {6, 8}, op->data[1]);
            InsertInteger(op, {2, 8}, varType(op->data[0])->_vector.base->size);
            runtimeApiHeader[GetHeaderId(ACTION_NEW_OBJECT, varType(op->data[0])->provider)].push_back({printCALL(op, 0x0) - assemblyCode, currentOrder});
            InsertMove(op, Register(op->data[0]), {7, 8}, false);
            break;
        }
        case OP_NEW_ARRAY:
        {
            // rdi=OBJECT_ARRAY=3
            // rsi=total size
            // rdx=size of element
            InsertInteger(op, {7, 8}, 0x03);
            ExternTo64Bit(op, Register(op->data[1]), isSigned(op->data[1]));
            printRRC(op, ASM_IMUL_RRC, {6, 8}, Register(op->data[1], 8), varType(op->data[0])->_vector.base->size);
            InsertInteger(op, {2, 8}, varType(op->data[0])->_vector.base->size);
            runtimeApiHeader[GetHeaderId(ACTION_NEW_OBJECT, varType(op->data[0])->provider)].push_back({printCALL(op, 0x0) - assemblyCode, currentOrder});
            InsertMove(op, Register(op->data[0]), {7, 8}, false);
            break;
        }
        case OP_NEW_PROMISE:
        {
            // rdi=OBJECT_PROMISE=2
            // rsi=total size
            // rdx=size of element = total size
            InsertInteger(op, {7, 8}, 0x02);
            InsertInteger(op, {6, 8}, varType(op->data[0])->_vector.base->size);
            InsertMove(op, {2, 8}, {6, 8}, false);
            runtimeApiHeader[GetHeaderId(ACTION_NEW_OBJECT, varType(op->data[0])->provider)].push_back({printCALL(op, 0x0) - assemblyCode, currentOrder});
            InsertMove(op, Register(op->data[0]), {7, 8}, false);
            break;
        }
        case OP_NEW_CLASS:
        {
            // rdi=OBJECT_OBJECT=4
            // rsi=total size
            // rdx=size of element = total size
            int64_t cls_size = GetClassSize(varType(op->data[0]));
            InsertInteger(op, {7, 8}, 0x04);
            InsertInteger(op, {6, 8}, cls_size);
            InsertMove(op, {2, 8}, {6, 8}, false);
            runtimeApiHeader[GetHeaderId(ACTION_NEW_OBJECT, varType(op->data[0])->provider)].push_back({printCALL(op, 0x0) - assemblyCode, currentOrder});
            InsertMove(op, Register(op->data[0]), {7, 8}, false);
            break;
        }
        case OP_NEW_PIPE:
        {
            // rdi=OBJECT_PIPE=1
            // rsi=total size
            // rdx=size of element = total size
            InsertInteger(op, {7, 8}, 0x01);
            InsertInteger(op, {6, 8}, varType(op->data[0])->_vector.base->size);
            InsertMove(op, {2, 8}, {6, 8}, false);
            runtimeApiHeader[GetHeaderId(ACTION_NEW_OBJECT, varType(op->data[0])->provider)].push_back({printCALL(op, 0x0) - assemblyCode, currentOrder});
            InsertMove(op, Register(op->data[0]), {7, 8}, false);
            break;
        }
        
        case OP_PUSH_VAR:
            // TODO: what if data[3] is not scalar?
            assert(op->data[1] != 0 || (TypeContext *)op->data[2] != varType(op->data[0]));
            // mov XX PTR [rbp + $0 + $1], $3
            printMR(op, ASM_MOV_MR, {5, 8}, Register(op->data[3]), memTable[op->data[0]] + op->data[1]);
            // (TypeContext *)op->data[2] - type [unused for now]
            break;
            
        case OP_PUSH_ARRAY:
        {
            if (varType(op->data[0])->provider == "loc")
            {
                int64_t elementSize = varType(op->data[0])->_vector.base->size;
                int64_t elementOffset = op->data[2];
                if (isApiScalar(op->data[4]))
                {
                    if (elementSize == 1 || elementSize == 2 || elementSize == 4 || elementSize == 8)
                    {
                        // mov [base + n * index + offset], reg
                        ExternTo64Bit(op, Register(op->data[1]), isSigned(op->data[1]));
                        printRCRCR(op, ASM_MOV_5MR, Register(op->data[0]), elementSize, Register(op->data[1], 8), elementOffset, Register(op->data[4]));
                    }
                    else
                    {
                        ExternTo64Bit(op, Register(op->data[1]), isSigned(op->data[1]));
                        printRRC(op, ASM_IMUL_RRC, {2, 8}, Register(op->data[1], 8), elementSize);
                        // mov [base + index + offset], reg
                        printRCRCR(op, ASM_MOV_5MR, Register(op->data[0]), 1, {2, 8}, elementOffset, Register(op->data[4]));
                    }
                }
                else
                {
                    logError(ir->filename, ir->code, op->code_start, op->code_end, "Local structures as element set aren't implemented [you can set their fields separately]");
                }
            }
            // TODO: remove usage of source as size provider
            // rcx=size rdx=offset rdi=object rsi=value
            else if (isApiScalar(op->data[4]))
            {
                InsertInteger(op, {1, 8}, -varSize(op->data[4]));
                ExternTo64Bit(op, Register(op->data[1]), isSigned(op->data[1]));
                printRRC(op, ASM_IMUL_RRC, {2, 8}, Register(op->data[1], 8), varType(op->data[0])->_vector.base->size);
                if (op->data[2] != 0) { printRC(op, ASM_ADD_RC, {2, 8}, op->data[2]); }
                InsertMove(op, {7, 8}, Register(op->data[0]), false);
                InsertMove(op, {6, 8}, Register(op->data[4]), false);
                runtimeApiHeader[GetHeaderId(ACTION_PUSH_OBJECT, varType(op->data[0])->provider)].push_back({printCALL(op, 0x0) - assemblyCode, currentOrder});
            }
            else
            {
                InsertInteger(op, {1, 8}, varSize(op->data[4]));
                ExternTo64Bit(op, Register(op->data[1]), isSigned(op->data[1]));
                printRRC(op, ASM_IMUL_RRC, {2, 8}, Register(op->data[1], 8), varType(op->data[0])->_vector.base->size);
                if (op->data[2] != 0) { printRC(op, ASM_ADD_RC, {2, 8}, op->data[2]); }
                InsertMove(op, {7, 8}, Register(op->data[0]), false);
                InsertInteger(op, {6, 8}, memTable[op->data[4]]);
                runtimeApiHeader[GetHeaderId(ACTION_PUSH_OBJECT, varType(op->data[0])->provider)].push_back({printCALL(op, 0x0) - assemblyCode, currentOrder});
            }
            break;
        }
        
        case OP_PUSH_PROMISE:
        {
            if (varType(op->data[0])->provider == "loc")
            {
                logError(ir->filename, ir->code, op->code_start, op->code_end, "Local promises are not implemented");
            }
            // TODO: remove usage of source as size provider
            // rcx=size rdx=offset rdi=object rsi=value
            if (isApiScalar(op->data[1]))
            {
                InsertInteger(op, {1, 8}, -varSize(op->data[1]));
                InsertInteger(op, {2, 8}, 0);
                InsertMove(op, {7, 8}, Register(op->data[0]), false);
                InsertMove(op, {6, 8}, Register(op->data[1]), false);
                runtimeApiHeader[GetHeaderId(ACTION_PUSH_OBJECT, varType(op->data[0])->provider)].push_back({printCALL(op, 0x0) - assemblyCode, currentOrder});
            }
            else
            {
                InsertInteger(op, {1, 8}, varSize(op->data[1]));
                InsertInteger(op, {2, 8}, 0);
                InsertMove(op, {7, 8}, Register(op->data[0]), false);
                InsertInteger(op, {6, 8}, memTable[op->data[1]]);
                runtimeApiHeader[GetHeaderId(ACTION_PUSH_OBJECT, varType(op->data[0])->provider)].push_back({printCALL(op, 0x0) - assemblyCode, currentOrder});
            }
            break;
        }
        
        case OP_PUSH_PIPE:
        {
            if (varType(op->data[0])->provider == "loc")
            {
                logError(ir->filename, ir->code, op->code_start, op->code_end, "Local pipes are not implemented");
            }
            // TODO: remove usage of source as size provider
            // rcx=size rdx=offset rdi=object rsi=value
            if (isApiScalar(op->data[1]))
            {
                InsertInteger(op, {1, 8}, -varSize(op->data[1]));
                InsertInteger(op, {2, 8}, 0);
                InsertMove(op, {7, 8}, Register(op->data[0]), false);
                InsertMove(op, {6, 8}, Register(op->data[1]), false);
                runtimeApiHeader[GetHeaderId(ACTION_PUSH_PIPE, varType(op->data[0])->provider)].push_back({printCALL(op, 0x0) - assemblyCode, currentOrder});
            }
            else
            {
                InsertInteger(op, {1, 8}, varSize(op->data[1]));
                InsertInteger(op, {2, 8}, 0);
                InsertMove(op, {7, 8}, Register(op->data[0]), false);
                InsertInteger(op, {6, 8}, memTable[op->data[1]]);
                runtimeApiHeader[GetHeaderId(ACTION_PUSH_PIPE, varType(op->data[0])->provider)].push_back({printCALL(op, 0x0) - assemblyCode, currentOrder});
            }
            break;
        }
        
        case OP_PUSH_CLASS:
        {
            if (varType(op->data[0])->provider == "loc")
            {
                int64_t elementOffset = op->data[1];
                if (isApiScalar(op->data[3]))
                {
                    printMR(op, ASM_MOV_MR, Register(op->data[0]), Register(op->data[3]), elementOffset);
                }
                else
                {
                    logError(ir->filename, ir->code, op->code_start, op->code_end, "Local classes can't assign structure field [you can set their fields separately]");
                }
            }
            // TODO: remove usage of source as size provider
            // rcx=size rdx=offset rdi=object rsi=value
            else if (isApiScalar(op->data[3]))
            {
                InsertInteger(op, {1, 8}, -varSize(op->data[3]));
                InsertInteger(op, {2, 8}, op->data[1]);
                InsertMove(op, {7, 8}, Register(op->data[0]), false);
                InsertMove(op, {6, 8}, Register(op->data[3]), false);
                runtimeApiHeader[GetHeaderId(ACTION_PUSH_OBJECT, varType(op->data[0])->provider)].push_back({printCALL(op, 0x0) - assemblyCode, currentOrder});
            }
            else
            {
                InsertInteger(op, {1, 8}, varSize(op->data[3]));
                InsertInteger(op, {2, 8}, op->data[1]);
                InsertMove(op, {7, 8}, Register(op->data[0]), false);
                InsertInteger(op, {6, 8}, memTable[op->data[4]]);
                runtimeApiHeader[GetHeaderId(ACTION_PUSH_OBJECT, varType(op->data[0])->provider)].push_back({printCALL(op, 0x0) - assemblyCode, currentOrder});
            }
            break;
        }
            
        case OP_QUERY_VAR: 
        {
            // TODO: what if data[3] is not scalar?
            assert(op->data[2] != 0 || (TypeContext *)op->data[3] != varType(op->data[0]));
            // mov $0, XX PTR [rbp + $1 + $2]
            printRM(op, ASM_MOV_RM, Register(op->data[0]), {5, 8}, memTable[op->data[1]] + op->data[2]);
            // (TypeContext *)op->data[3] - type [unused for now]
            break;
        }
            
        case OP_QUERY_INDEX:
        {
            if (varType(op->data[1])->provider == "loc")
            {
                int64_t elementSize = varType(op->data[1])->_vector.base->size;
                int64_t elementOffset = op->data[2];
                if (isApiScalar(op->data[0]))
                {
                    if (elementSize == 1 || elementSize == 2 || elementSize == 4 || elementSize == 8)
                    {
                        // mov reg, [base + n * index + offset]
                        ExternTo64Bit(op, Register(op->data[4]), isSigned(op->data[4]));
                        printRRCRC(op, ASM_MOV_5RM, Register(op->data[0]), Register(op->data[1]), elementSize, Register(op->data[4], 8), elementOffset);
                    }
                    else
                    {
                        ExternTo64Bit(op, Register(op->data[4]), isSigned(op->data[4]));
                        printRRC(op, ASM_IMUL_RRC, {2, 8}, Register(op->data[4], 8), elementSize);
                        // mov [base + index + offset], reg
                        printRRCRC(op, ASM_MOV_5RM, Register(op->data[0]), Register(op->data[1]), 1, {2, 8}, elementOffset);
                    }
                }
                else
                {
                    logError(ir->filename, ir->code, op->code_start, op->code_end, "Local structures as element get aren't implemented [you can set their fields separately]");
                }
            }
            // TODO: remove usage of destination as size provider
            // rcx=size rdx=offset rdi=value rsi=object
            else if (isApiScalar(op->data[0]))
            {
                InsertInteger(op, {1, 8}, -varSize(op->data[0]));
                ExternTo64Bit(op, Register(op->data[4]), isSigned(op->data[4]));
                printRRC(op, ASM_IMUL_RRC, {2, 8}, Register(op->data[4], 8), varType(op->data[1])->_vector.base->size);
                if (op->data[2] != 0) { printRC(op, ASM_ADD_RC, {2, 8}, op->data[2]); }
                InsertMove(op, {6, 8}, Register(op->data[1]), false);
                runtimeApiHeader[GetHeaderId(ACTION_QUERY_OBJECT, varType(op->data[1])->provider)].push_back({printCALL(op, 0x0) - assemblyCode, currentOrder});
                InsertMove(op, Register(op->data[0]), {7, 8}, false);
            }
            else
            {
                InsertInteger(op, {1, 8}, varSize(op->data[0]));
                ExternTo64Bit(op, Register(op->data[4]), isSigned(op->data[4]));
                printRRC(op, ASM_IMUL_RRC, {2, 8}, Register(op->data[4], 8), varType(op->data[1])->_vector.base->size);
                if (op->data[2] != 0) { printRC(op, ASM_ADD_RC, {2, 8}, op->data[2]); }
                InsertInteger(op, {7, 8}, memTable[op->data[0]]);
                InsertMove(op, {6, 8}, Register(op->data[4]), false);
                runtimeApiHeader[GetHeaderId(ACTION_QUERY_OBJECT, varType(op->data[1])->provider)].push_back({printCALL(op, 0x0) - assemblyCode, currentOrder});
            }
            break;
        }
        
        case OP_QUERY_ARRAY:
        {
            if (varType(op->data[1])->provider == "loc")
            {
                printRM(op, ASM_MOV_RM, Register(op->data[0]), Register(op->data[1]), -16);
            }
            // rcx=size rdx=offset rdi=value rsi=object
            // TODO: remove usage of destination as size provider
            else if (isApiScalar(op->data[0]))
            {
                InsertInteger(op, {1, 8}, -varSize(op->data[0]));
                InsertInteger(op, {2, 8}, -16);
                InsertMove(op, {6, 8}, Register(op->data[1]), false);
                runtimeApiHeader[GetHeaderId(ACTION_QUERY_OBJECT, varType(op->data[1])->provider)].push_back({printCALL(op, 0x0) - assemblyCode, currentOrder});
                InsertMove(op, Register(op->data[0]), {7, 8}, false);
            }
            else
            {
                InsertInteger(op, {1, 8}, varSize(op->data[0]));
                InsertInteger(op, {2, 8}, -16);
                InsertInteger(op, {7, 8}, memTable[op->data[0]]);
                InsertMove(op, {6, 8}, Register(op->data[1]), false);
                runtimeApiHeader[GetHeaderId(ACTION_QUERY_OBJECT, varType(op->data[1])->provider)].push_back({printCALL(op, 0x0) - assemblyCode, currentOrder});
            }
            break;
        }
        
        case OP_QUERY_PROMISE:
        {
            if (varType(op->data[1])->provider == "loc")
            {
                logError(ir->filename, ir->code, op->code_start, op->code_end, "Local promises are not implemented");
            }
            // TODO: remove usage of destination as size provider
            // rcx=size rdx=offset rdi=value rsi=object
            if (isApiScalar(op->data[0]))
            {
                InsertInteger(op, {1, 8}, -varSize(op->data[0]));
                InsertInteger(op, {2, 8}, 0);
                InsertMove(op, {6, 8}, Register(op->data[1]), false);
                runtimeApiHeader[GetHeaderId(ACTION_QUERY_OBJECT, varType(op->data[1])->provider)].push_back({printCALL(op, 0x0) - assemblyCode, currentOrder});
                InsertMove(op, Register(op->data[0]), {7, 8}, false);
            }
            else
            {
                InsertInteger(op, {1, 8}, varSize(op->data[0]));
                InsertInteger(op, {2, 8}, 0);
                InsertInteger(op, {7, 8}, memTable[op->data[0]]);
                InsertMove(op, {6, 8}, Register(op->data[1]), false);
                runtimeApiHeader[GetHeaderId(ACTION_QUERY_OBJECT, varType(op->data[1])->provider)].push_back({printCALL(op, 0x0) - assemblyCode, currentOrder});
            }
            break;
        }
        
        case OP_QUERY_CLASS:
        {
            if (varType(op->data[1])->provider == "loc")
            {
                int64_t elementOffset = op->data[2];
                if (isApiScalar(op->data[0]))
                {
                    printRM(op, ASM_MOV_RM, Register(op->data[0]), Register(op->data[1]), elementOffset);
                }
                else
                {
                    logError(ir->filename, ir->code, op->code_start, op->code_end, "Local classes can't load structure field [you can set their fields separately]");
                }
            }
            // TODO: remove usage of destination as size provider
            // rcx=size rdx=offset rdi=value rsi=object
            else if (isApiScalar(op->data[0]))
            {
                InsertInteger(op, {1, 8}, -varSize(op->data[0]));
                InsertInteger(op, {2, 8}, op->data[2]);
                InsertMove(op, {6, 8}, Register(op->data[1]), false);
                runtimeApiHeader[GetHeaderId(ACTION_QUERY_OBJECT, varType(op->data[1])->provider)].push_back({printCALL(op, 0x0) - assemblyCode, currentOrder});
                InsertMove(op, Register(op->data[0]), {7, 8}, false);
            }
            else
            {
                InsertInteger(op, {1, 8}, varSize(op->data[0]));
                InsertInteger(op, {2, 8}, op->data[2]);
                InsertInteger(op, {7, 8}, memTable[op->data[0]]);
                InsertMove(op, {6, 8}, Register(op->data[1]), false);
                runtimeApiHeader[GetHeaderId(ACTION_QUERY_OBJECT, varType(op->data[1])->provider)].push_back({printCALL(op, 0x0) - assemblyCode, currentOrder});
            }
            break;
        }
        
        case OP_QUERY_PIPE:
        {
            if (varType(op->data[1])->provider == "loc")
            {
                logError(ir->filename, ir->code, op->code_start, op->code_end, "Local pipes are not implemented");
            }
            // TODO: remove usage of destination as size provider
            // rcx=size rdx=offset rdi=value rsi=object
            if (isApiScalar(op->data[0]))
            {
                InsertInteger(op, {1, 8}, -varSize(op->data[0]));
                InsertInteger(op, {2, 8}, 0);
                InsertMove(op, {6, 8}, Register(op->data[1]), false);
                runtimeApiHeader[GetHeaderId(ACTION_QUERY_PIPE, varType(op->data[1])->provider)].push_back({printCALL(op, 0x0) - assemblyCode, currentOrder});
                InsertMove(op, Register(op->data[0]), {7, 8}, false);
            }
            else
            {
                InsertInteger(op, {1, 8}, varSize(op->data[0]));
                InsertInteger(op, {2, 8}, 0);
                InsertInteger(op, {7, 8}, memTable[op->data[0]]);
                InsertMove(op, {6, 8}, Register(op->data[1]), false);
                runtimeApiHeader[GetHeaderId(ACTION_QUERY_PIPE, varType(op->data[1])->provider)].push_back({printCALL(op, 0x0) - assemblyCode, currentOrder});
            }
            break;
        }
         
        case OP_BOR:   ABEL_BINOP(ASM_OR) break;
        case OP_BAND:  ABEL_BINOP(ASM_AND) break;
        case OP_BXOR:  ABEL_BINOP(ASM_XOR) break;

        // TODO: add variant without BMI2
        case OP_SHL:   
            // shlx $0 $1 $2
            printRRR(op, ASM_SHLX, Register(op->data[0]), Register(op->data[1]), Register(op->data[2]));
            break;
        case OP_SHR:
            // shrx $0 $1 $2
            printRRR(op, ASM_SHRX, Register(op->data[0]), Register(op->data[1]), Register(op->data[2]));
            break;
        
        case OP_BNOT:
            InsertMove(op, op->data[0], op->data[2]);
            printR(op, ASM_NOT, Register(op->data[0]));
            break;
        
        case OP_ADD:   ABEL_BINOP(ASM_ADD) break;
        case OP_MUL:   ABEL_BINOP(ASM_IMUL) break;
        case OP_SUB:
            if (regTable[op->data[0]] == regTable[op->data[2]])
            {
                printRR(op, ASM_SUB, Register(op->data[0]), Register(op->data[1]));
                printR(op, ASM_NEG, Register(op->data[0]));
            }
            else
            {
                InsertMove(op, op->data[0], op->data[1]);
                printRR(op, ASM_SUB, Register(op->data[0]), Register(op->data[2]));
            }
            break;
        
        case OP_EQ:    CMPOP(ASM_SETE,  ASM_SETE)  break;
        case OP_NE:    CMPOP(ASM_SETNE, ASM_SETNE) break;
        case OP_LT:    CMPOP(ASM_SETL,  ASM_SETB)  break;
        case OP_LE:    CMPOP(ASM_SETLE, ASM_SETBE) break;
        case OP_GT:    CMPOP(ASM_SETG,  ASM_SETA)  break;
        case OP_GE:    CMPOP(ASM_SETGE, ASM_SETAE) break;

        case OP_DIV:
            InsertMove(op, {0, 8}, Register(op->data[1]), isSigned(op->data[0]));            // mov rax, $1
            if (isSigned(op->data[0])) 
                printZ(op, ASM_CQO);                                                         // cqo
            else 
                printRR(op, ASM_XOR, {2, 4}, {2, 4});                                        // xor edx, edx
            printR(op, (isSigned(op->data[0]) ? ASM_IDIV : ASM_DIV), Register(op->data[2])); // div $2
            InsertMove(op, Register(op->data[0]), {0, 8}, false);                            // mov $0, rax ; sign doesn't matter
            break;
            
        case OP_MOD:
            InsertMove(op, {0, 8}, Register(op->data[1]), isSigned(op->data[0]));            // mov rax, $1
            if (isSigned(op->data[0])) 
                printZ(op ,ASM_CQO);                                                         // cqo
            else 
                printRR(op, ASM_XOR, {2, 4}, {2, 4});                                        // xor edx, edx
            printR(op, (isSigned(op->data[0]) ? ASM_IDIV : ASM_DIV), Register(op->data[2])); // div $2
            InsertMove(op, Register(op->data[0]), {2, 8}, false);                            // mov $0, rdx ; sign doesn't matter
            break;
    }

    for (auto &n : views::reverse(op->next))
    {
        toBuild.push_back({n, op});
    }
}
