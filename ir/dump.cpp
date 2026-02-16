#include "utils.hpp"
#include "../ir.hpp"


char *printTypeR(char *text, TypeContext *t)
{
    switch (t->type)
    {
        case TYPE_UNION: return text + sprintf(text, "union@%s", t->provider.c_str()); break;
        case TYPE_RECORD: return text + sprintf(text, "record@%s", t->provider.c_str()); break;
        case TYPE_CLASS: return text + sprintf(text, "class@%s", t->provider.c_str()); break;
        case TYPE_PIPE: text += sprintf(text, "pipe@%s of ", t->provider.c_str()); return printTypeR(text, t->_vector.base); break;
        case TYPE_ARRAY: text += sprintf(text, "array@%s of ", t->provider.c_str()); return printTypeR(text, t->_vector.base); break;
        case TYPE_PROMISE: text += sprintf(text, "promise@%s of ", t->provider.c_str()); return printTypeR(text, t->_vector.base); break;
        case TYPE_SCALAR: return text + sprintf(text, "scalar@%s [%d, size=%lld]", t->provider.c_str(), t->_scalar.kind, t->size); break;
    }
}


char _printTypeText[4*4096];
string printType(TypeContext *t)
{
    printTypeR(_printTypeText, t);
    return string(_printTypeText);
}

set<OperationBlock *> dump_used;
void dumpIRR(WorkerDeclarationContext *fn, OperationBlock *x)
{
    if (!x || !dump_used.insert(x).second)
    {
        return;
    }
    printf("%p ", x);
    switch (x->type)
    {
        case OP_SLEEP: printf("OP_SLEEP "); break;
        
        case OP_LOAD_INPUT: printf("OP_LOAD_INPUT "); break;
        case OP_LOAD_OUTPUT: printf("OP_LOAD_OUTPUT "); break;

        case OP_FREE_TEMP: printf("OP_FREE_TEMP "); break;

        case OP_STORE: printf("OP_STORE "); break;
        case OP_LOAD: printf("OP_LOAD "); break;
        case OP_STORE_INPUT: printf("OP_STORE_INPUT "); break;

        case OP_CALL: printf("OP_CALL "); break;
        case OP_CAST: printf("OP_CAST "); break;
        case OP_MOV: printf("OP_MOV "); break;

        case OP_NEW_INT: printf("OP_NEW_INT "); break;
        case OP_NEW_FLOAT: printf("OP_NEW_FLOAT "); break;
        case OP_NEW_ARRAY: printf("OP_NEW_ARRAY "); break;
        case OP_NEW_PIPE: printf("OP_NEW_PIPE "); break;
        case OP_NEW_PROMISE: printf("OP_NEW_PROMISE "); break;
        case OP_NEW_CLASS: printf("OP_NEW_CLASS "); break;
        case OP_NEW_STRING: printf("OP_NEW_STRING "); break;

        case OP_PUSH_VAR: printf("OP_PUSH_VAR "); break;
        case OP_PUSH_ARRAY: printf("OP_PUSH_ARRAY "); break;
        case OP_PUSH_PIPE: printf("OP_PUSH_PIPE "); break;
        case OP_PUSH_PROMISE: printf("OP_PUSH_PROMISE "); break;
        case OP_PUSH_CLASS: printf("OP_PUSH_CLASS "); break;
        case OP_QUERY_VAR: printf("OP_QUERY_VAR "); break;
        case OP_QUERY_ARRAY: printf("OP_QUERY_ARRAY "); break;
        case OP_QUERY_INDEX: printf("OP_QUERY_INDEX "); break;
        case OP_QUERY_PIPE: printf("OP_QUERY_PIPE "); break;
        case OP_QUERY_PROMISE: printf("OP_QUERY_PROMISE "); break;
        case OP_QUERY_CLASS: printf("OP_QUERY_CLASS "); break;

        case OP_BOR: printf("OP_BOR "); break;
        case OP_BAND: printf("OP_BAND "); break;
        case OP_BXOR: printf("OP_BXOR "); break;
        case OP_SHL: printf("OP_SHL "); break;
        case OP_SHR: printf("OP_SHR "); break;
        case OP_BNOT: printf("OP_BNOT "); break;

        case OP_JMP: printf("OP_JMP "); break;
        case OP_JZ: printf("OP_JZ "); break;
        case OP_JNZ: printf("OP_JNZ "); break;

        case OP_ADD: printf("OP_ADD "); break;
        case OP_SUB: printf("OP_SUB "); break;
        case OP_MUL: printf("OP_MUL "); break;
        case OP_DIV: printf("OP_DIV "); break;
        case OP_MOD: printf("OP_MOD "); break;
        case OP_EQ: printf("OP_EQ "); break;
        case OP_NE: printf("OP_NE "); break;
        case OP_LT: printf("OP_LT "); break;
        case OP_LE: printf("OP_LE "); break;
        case OP_GT: printf("OP_GT "); break;
        case OP_GE: printf("OP_GE "); break;
    }
    printf("[ ");
    for (int64_t t : x->data)
    {
        printf("%lld ", t);
    }
    printf("]");
    for (int64_t i = 0; i < (int64_t)x->next.size(); ++i)
    {
        printf(" next=%p ", x->next[i]);
    }
    for (auto &i : x->prev)
    {
        printf(" [prev=%p] ", i);
    }
    if (x->attributes.size() > 0)
    {
        printf(" { ");
        for (auto &[k, v] : x->attributes)
        {
            visit(overload{
                [&](const string &str){
                    printf("%s=%s ", k.c_str(), str.c_str());
                },
                [&](const int64_t &val){
                    printf("%s=%lld [type=%s] ", k.c_str(), val, printType(fn->content->variables[val]).c_str());
                }
            }, v);
        }
        printf("}");
    }
    printf("\n");

    for (auto &y : x->next)
    {
        dumpIRR(fn, y);
    }
}

void dumpIR(WorkerDeclarationContext *fn)
{
    dump_used.clear();
    printf("Worker %s\n", fn->name.c_str());
    if (fn->content)
    {
        printf("code:\n");
        for (auto &x : fn->content->code)
        {
            dumpIRR(fn, x);
        }
    }
    dump_used.clear();
}
