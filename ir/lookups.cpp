#include "utils.hpp"
#include "../ir.hpp"

void applyNamesTranslition(OperationBlock *op, const map<int64_t, int64_t> &translition)
{
    #define T(x) \
        if (translition.contains(x)) x = translition.find(x)->second;

    switch (op->type)
    {
        // 2nd
        case OP_LOAD_INPUT:
        case OP_LOAD_OUTPUT:
            T(op->data[1]);
            break;

        // 1, last
        case OP_PUSH_VAR:
        case OP_PUSH_CLASS:
            T(op->data[0]);
            T(op->data.back());
            break;

        // 1, 2
        case OP_QUERY_VAR:
        case OP_QUERY_CLASS:
            T(op->data[0]);
            T(op->data[1]);
            break;

        // 1, 2 and last
        case OP_PUSH_ARRAY:
        case OP_QUERY_INDEX:
            T(op->data[0]);
            T(op->data[1]);
            T(op->data.back());
            break;

        // all except first
        case OP_CALL:
            for (auto &i : op->data | views::drop(1))
            {
                T(i);
            }
            break;

        // first arg
        case OP_STORE:
        case OP_STORE_INPUT:
        case OP_LOAD:
        case OP_NEW_INT:
        case OP_NEW_STRING:
        case OP_NEW_FLOAT:
            T(op->data[0]);
            break;

        // all args
        case OP_SLEEP:
        case OP_FREE_TEMP:
        case OP_JMP:
        case OP_JZ:
        case OP_JNZ:
        case OP_PUSH_PIPE:
        case OP_PUSH_PROMISE:
        case OP_QUERY_ARRAY:
        case OP_QUERY_PIPE:
        case OP_QUERY_PROMISE:
        case OP_NEW_ARRAY:
        case OP_NEW_PIPE:
        case OP_NEW_PROMISE:
        case OP_NEW_CLASS:
        case OP_CAST:
        case OP_MOV:
        case OP_BOR:
        case OP_BAND:
        case OP_BXOR:
        case OP_SHL:
        case OP_SHR:
        case OP_BNOT:
        case OP_ADD:
        case OP_SUB:
        case OP_MUL:
        case OP_DIV:
        case OP_MOD:
        case OP_NE:
        case OP_EQ:
        case OP_LT:
        case OP_LE:
        case OP_GT:
        case OP_GE:
            for (auto &i : op->data)
            {
                T(i);
            }
            break;
    }

    for (auto &[k, v] : op->attributes)
    {
        if (int64_t *x = get_if<int64_t>(&v))
        {
            T(*x);
        }
    }

    #undef T
}

vector<int64_t> getWritedVariables(OperationBlock *op)
{
    vector<int64_t> result;
    switch (op->type)
    {
        // 2nd
        case OP_LOAD_INPUT:
        case OP_LOAD_OUTPUT:
            return {op->data[1]};

        case OP_CALL:
        case OP_FREE_TEMP:
        case OP_SLEEP:
        case OP_PUSH_PIPE:
        case OP_PUSH_PROMISE:
        case OP_PUSH_CLASS:
        case OP_PUSH_ARRAY:
        case OP_JMP:
        case OP_JZ:
        case OP_JNZ:
        case OP_STORE:
        case OP_STORE_INPUT:
            return {};

        // first arg
        case OP_QUERY_INDEX:
        case OP_QUERY_ARRAY:
        case OP_QUERY_PIPE:
        case OP_QUERY_PROMISE:
        case OP_QUERY_VAR:
        case OP_QUERY_CLASS:
        case OP_LOAD:
        case OP_PUSH_VAR:
        case OP_NEW_INT:
        case OP_NEW_FLOAT:
        case OP_NEW_ARRAY:
        case OP_NEW_STRING:
        case OP_NEW_PIPE:
        case OP_NEW_PROMISE:
        case OP_NEW_CLASS:
        case OP_CAST:
        case OP_MOV:
        case OP_BOR:
        case OP_BAND:
        case OP_BXOR:
        case OP_SHL:
        case OP_SHR:
        case OP_BNOT:
        case OP_ADD:
        case OP_SUB:
        case OP_MUL:
        case OP_DIV:
        case OP_MOD:
        case OP_NE:
        case OP_EQ:
        case OP_LT:
        case OP_LE:
        case OP_GT:
        case OP_GE:
            return {op->data[0]};
    }
}

vector<int64_t> getUsedVariables(OperationBlock *op)
{
    vector<int64_t> result;
    switch (op->type)
    {
        // 2nd
        case OP_LOAD_INPUT:
        case OP_LOAD_OUTPUT:
            result = {op->data[1]}; break;

        // 1, last
        case OP_PUSH_VAR:
        case OP_PUSH_CLASS:
            result = {op->data[0], op->data.back()}; break;

        // 1, 2
        case OP_QUERY_VAR:
        case OP_QUERY_CLASS:
            result = {op->data[0], op->data[1]}; break;

        // 1, 2 and last
        case OP_PUSH_ARRAY:
        case OP_QUERY_INDEX:
            result = {op->data[0], op->data[1], op->data.back()}; break;

        // all except first
        case OP_CALL:
        {
            auto t = op->data | views::drop(1);
            result = vector<int64_t>(t.begin(), t.end()); break;
        }

        // first arg
        case OP_STORE:
        case OP_STORE_INPUT:
        case OP_LOAD:
        case OP_NEW_CLASS:
        case OP_NEW_STRING:
        case OP_NEW_INT:
        case OP_NEW_FLOAT:
            result = {op->data[0]}; break;

        // all args
        case OP_FREE_TEMP:
        case OP_SLEEP:
        case OP_JMP:
        case OP_JZ:
        case OP_JNZ:
        case OP_PUSH_PIPE:
        case OP_PUSH_PROMISE:
        case OP_QUERY_ARRAY:
        case OP_QUERY_PIPE:
        case OP_QUERY_PROMISE:
        case OP_NEW_ARRAY:
        case OP_NEW_PIPE:
        case OP_NEW_PROMISE:
        case OP_CAST:
        case OP_MOV:
        case OP_BOR:
        case OP_BAND:
        case OP_BXOR:
        case OP_SHL:
        case OP_SHR:
        case OP_BNOT:
        case OP_ADD:
        case OP_SUB:
        case OP_MUL:
        case OP_DIV:
        case OP_MOD:
        case OP_EQ:
        case OP_NE:
        case OP_LT:
        case OP_LE:
        case OP_GT:
        case OP_GE:
            result = op->data; break;
    }

    for (auto &[k, v] : op->attributes)
    {
        if (int64_t *x = get_if<int64_t>(&v))
        {
            result.push_back(*x);
        }
    }

    return result;
}

vector<int64_t> getReadVariables(OperationBlock *op)
{
    vector<int64_t> result;
    switch (op->type)
    {
        // none
        case OP_JMP:
        case OP_LOAD_INPUT:
        case OP_LOAD_OUTPUT:
        case OP_LOAD:
        case OP_NEW_CLASS:
        case OP_NEW_INT:
        case OP_NEW_FLOAT:
        case OP_NEW_STRING:
        case OP_NEW_PIPE:
        case OP_NEW_PROMISE:
            result = {}; break;

        // first and last
        case OP_PUSH_PIPE:
        case OP_PUSH_PROMISE:
        case OP_PUSH_CLASS:
            result = {op->data[0], op->data.back()}; break;

        // last
        case OP_PUSH_VAR:
            result = {op->data.back()}; break;

        // 2
        case OP_QUERY_VAR:
        case OP_QUERY_CLASS:
            result = {op->data[1]}; break;

        // 1, 2 and last
        case OP_PUSH_ARRAY:
            result = {op->data[0], op->data[1], op->data.back()}; break;

        // 2 and last
        case OP_QUERY_INDEX:
            result = {op->data[1], op->data.back()}; break;


        // first arg
        case OP_JZ:
        case OP_JNZ:
        case OP_SLEEP:
        case OP_STORE:
        case OP_STORE_INPUT:
            result = {op->data[0]}; break;


        // all except first
        case OP_CALL:
        case OP_FREE_TEMP:
        case OP_QUERY_ARRAY:
        case OP_QUERY_PIPE:
        case OP_QUERY_PROMISE:
        case OP_NEW_ARRAY:
        case OP_CAST:
        case OP_MOV:
        case OP_BOR:
        case OP_BAND:
        case OP_BXOR:
        case OP_SHL:
        case OP_SHR:
        case OP_BNOT:
        case OP_ADD:
        case OP_SUB:
        case OP_MUL:
        case OP_DIV:
        case OP_MOD:
        case OP_NE:
        case OP_EQ:
        case OP_LT:
        case OP_LE:
        case OP_GT:
        case OP_GE:
        {
            auto t = op->data | views::drop(1);
            result = vector<int64_t>(t.begin(), t.end()); break;
        }
    }

    for (auto &[k, v] : op->attributes)
    {
        if (int64_t *x = get_if<int64_t>(&v))
        {
            result.push_back(*x);
        }
    }

    return result;
}
