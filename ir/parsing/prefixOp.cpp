#include "../../ir.hpp"
#include "../utils.hpp"


pair<vector<Operation>, int64_t> buildPrefixOperator(BuildContext *ctx, Node *node)
{
    vector<Operation> ops;
    
    auto [t, pos] = buildPrefixOperation(ctx, node->nonTerm(1));
    HANDLE_NOT_NULL(pos, node->nonTerm(1));

    int64_t resultPosition = pos;

    append(ops, t);

    switch_var(node->nonTerm(0))
    {
        case 0: // +
        { return {t, resultPosition}; }
        case 1: // -
        {
            int64_t tmp = newTemp(ctx, ctx->variables[pos]);
            append(ops, {OP_NEW_INT, {tmp, 0}, {}, node->start, node->end});
            append(ops, {OP_SUB, {tmp, tmp, pos}, {}, node->start, node->end});
            freeTemp(ops, pos);
            return {ops, tmp};
        }
        case 2: // !
        {
            int64_t tmp = newTemp(ctx, getBaseType(ctx, "i32"));
            append(ops, {OP_JZ, {3, pos}, {}, node->start, node->end});
            append(ops, {OP_NEW_INT, {tmp, -1}, {}, node->start, node->end});
            append(ops, {OP_JMP, {2, pos}, {}, node->start, node->end});
            append(ops, {OP_NEW_INT, {tmp, 0}, {}, node->start, node->end});
            freeTemp(ops, pos);
            return {ops, tmp};
        }
        case 3: // ~
        {  
            int64_t tmp = newTemp(ctx, ctx->variables[pos]);
            append(ops, {OP_BNOT, {tmp, pos}, {}, node->start, node->end}); 
            freeTemp(ops, pos);
            return {ops, tmp};
        }
    }
    return {{}, -1};
}


pair<vector<Operation>, int64_t> buildCastOperator(BuildContext *ctx, Node *node)
{
    vector<Operation> ops;
    
    auto [t, pos] = buildPrefixOperation(ctx, node->nonTerm(1));
    HANDLE_NOT_NULL(pos, node->nonTerm(1));

    TypeContext *type = getType(ctx, node->nonTerm(0));
    if (type == NULL)
    {
        return {{}, -1};
    }

    if (!is_castable(type, ctx->variables[pos]))
    {
        logError(ctx->filename, ctx->code, node->start, node->end, "Cast is impossible between %s and %s", printType(type).c_str(), printType(ctx->variables[pos]).c_str());
    }

    append(ops, t);
    int64_t tmp = newTemp(ctx, type);
    append(ops, {OP_CAST, {tmp, pos}, {}, node->start, node->end});
    freeTemp(ops, pos);

    return {ops, tmp};
}


pair<vector<Operation>, int64_t> buildPrefixOperation(BuildContext *ctx, Node *node)
{
    if (!is(node, "PrefixOperation"))
    {
        return buildQueryOperation(ctx, node);
    }
    printf("Prefix operation\n");

    switch_var(node)
    {
        case 0: // operator
        {
            return buildPrefixOperator(ctx, node);
        }
        case 2: // cast
        case 3: // cast
        {
            return buildCastOperator(ctx, node);
        }
    }
    return {{}, -1};
}


