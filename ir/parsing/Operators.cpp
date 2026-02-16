#include "../../ir.hpp"
#include "../utils.hpp"


pair<vector<Operation>, int64_t> buildBinOperation(BuildContext *ctx, Node *node)
{
    if (!is(node, "BinOperation"))
    {
        return buildPrefixOperation(ctx, node);
    }
    printf("Bin operation\n");
    vector<Operation> ops;
    auto [t, pos] = buildPrefixOperation(ctx, node->nonTerm(0));
    HANDLE_NOT_NULL(pos, node->nonTerm(0));
    int64_t id = 1;
    while (node->nonTerm(id))
    {
        auto [t2, pos2] = buildPrefixOperation(ctx, node->nonTerm(id + 1));
        HANDLE_NOT_NULL(pos2, node->nonTerm(id + 1));

        auto [can, resType] = operation_types(ctx->variables[pos], ctx->variables[pos2]);
        if (!can)
        {
            logError(ctx->filename, ctx->code, node->nonTerm(id + 0)->start, node->nonTerm(id + 0)->end, "Can't implictly process binary operation on %s and %s", printType(ctx->variables[pos]).c_str(), printType(ctx->variables[pos2]).c_str());
            return {{}, -1};
        }

        int64_t tmp = newTemp(ctx, resType);

        append(ops, t);
        append(ops, t2);

        switch_var(node->nonTerm(id + 0))
        {
            case 0: // |
            { append(ops, {OP_BOR, {tmp, pos, pos2}, {}, node->start, node->end}); break; }
            case 1: // &
            { append(ops, {OP_BAND, {tmp, pos, pos2}, {}, node->start, node->end}); break; }
            case 2: // ^
            { append(ops, {OP_BXOR, {tmp, pos, pos2}, {}, node->start, node->end}); break; }
            case 3: // <<
            { append(ops, {OP_SHL, {tmp, pos, pos2}, {}, node->start, node->end}); break; }
            case 4: // >>
            { append(ops, {OP_SHR, {tmp, pos, pos2}, {}, node->start, node->end}); break; }
        }

        freeTemp(ops, pos2);
        freeTemp(ops, pos);

        pos = tmp;
        t = ops;
        ops.clear();
        id += 2;
    }
    return {t, pos};
}

pair<vector<Operation>, int64_t> buildMulOperation(BuildContext *ctx, Node *node)
{
    if (!is(node, "MulOperation"))
    {
        return buildBinOperation(ctx, node);
    }
    printf("Mul operation\n");
    vector<Operation> ops;
    auto [t, pos] = buildBinOperation(ctx, node->nonTerm(0));
    HANDLE_NOT_NULL(pos, node->nonTerm(0));
    int64_t id = 1;
    while (node->nonTerm(id))
    {
        auto [t2, pos2] = buildBinOperation(ctx, node->nonTerm(id + 1));
        HANDLE_NOT_NULL(pos2, node->nonTerm(id + 1));

        auto [can, resType] = operation_types(ctx->variables[pos], ctx->variables[pos2]);
        if (!can)
        {
            logError(ctx->filename, ctx->code, node->nonTerm(id + 0)->start, node->nonTerm(id + 0)->end, "Can't implictly multiplicate %s and %s", printType(ctx->variables[pos]).c_str(), printType(ctx->variables[pos2]).c_str());
            return {{}, -1};
        }

        int64_t tmp = newTemp(ctx, resType);

        append(ops, t);
        append(ops, t2);

        switch_var(node->nonTerm(id + 0))
        {
            case 0: // *
            { append(ops, {OP_MUL, {tmp, pos, pos2}, {}, node->start, node->end}); break; }
            case 1: // /
            { append(ops, {OP_DIV, {tmp, pos, pos2}, {}, node->start, node->end}); break; }
            case 2: // %
            { append(ops, {OP_MOD, {tmp, pos, pos2}, {}, node->start, node->end}); break; }
        }

        freeTemp(ops, pos2);
        freeTemp(ops, pos);

        pos = tmp;
        t = ops;
        ops.clear();
        id += 2;
    }
    return {t, pos};
}

pair<vector<Operation>, int64_t> buildAddOperation(BuildContext *ctx, Node *node)
{
    if (!is(node, "AddOperation"))
    {
        return buildMulOperation(ctx, node);
    }
    printf("Add operation\n");
    vector<Operation> ops;
    auto [t, pos] = buildMulOperation(ctx, node->nonTerm(0));
    HANDLE_NOT_NULL(pos, node->nonTerm(0));
    int64_t id = 1;
    while (node->nonTerm(id))
    {
        auto [t2, pos2] = buildMulOperation(ctx, node->nonTerm(id + 1));
        HANDLE_NOT_NULL(pos2, node->nonTerm(id + 1));

        auto [can, resType] = operation_types(ctx->variables[pos], ctx->variables[pos2]);
        if (!can)
        {
            logError(ctx->filename, ctx->code, node->nonTerm(id + 0)->start, node->nonTerm(id + 0)->end, "Can't implictly multiplicate %s and %s", printType(ctx->variables[pos]).c_str(), printType(ctx->variables[pos2]).c_str());
            return {{}, -1};
        }

        int64_t tmp = newTemp(ctx, resType);

        append(ops, t);
        append(ops, t2);

        switch_var(node->nonTerm(id + 0))
        {
            case 0: // +
            { append(ops, {OP_ADD, {tmp, pos, pos2}, {}, node->start, node->end}); break; }
            case 1: // -
            { append(ops, {OP_SUB, {tmp, pos, pos2}, {}, node->start, node->end}); break; }
        }

        freeTemp(ops, pos2);
        freeTemp(ops, pos);

        pos = tmp;
        t = ops;
        ops.clear();
        id += 2;
    }
    return {t, pos};
}

pair<vector<Operation>, int64_t> buildCompareOperation(BuildContext *ctx, Node *node)
{
    if (!is(node, "CompareOperation"))
    {
        return buildAddOperation(ctx, node);
    }
    printf("Compare operation\n");
    vector<Operation> ops;
    auto [t, pos] = buildAddOperation(ctx, node->nonTerm(0));
    HANDLE_NOT_NULL(pos, node->nonTerm(0));
    int64_t id = 1;
    while (node->nonTerm(id))
    {
        auto [t2, pos2] = buildAddOperation(ctx, node->nonTerm(id + 1));
        HANDLE_NOT_NULL(pos2, node->nonTerm(id + 1));

        if (ctx->variables[pos]->provider != ctx->variables[pos2]->provider)
        {
            logError(ctx->filename, ctx->code, node->nonTerm(id + 0)->start, node->nonTerm(id + 0)->end, "Can't implictly compare %s and %s [providers differs]", printType(ctx->variables[pos]).c_str(), printType(ctx->variables[pos2]).c_str());
            return {{}, -1};
        }

        int64_t tmp = newTemp(ctx, getBaseType(ctx, "i32"));

        append(ops, t);
        append(ops, t2);

        switch_var(node->nonTerm(id + 0))
        {
            case 0: // <>
            { append(ops, {OP_NE, {tmp, pos, pos2}, {}, node->start, node->end}); break; }
            case 1: // >=
            { append(ops, {OP_GE, {tmp, pos, pos2}, {}, node->start, node->end}); break; }
            case 2: // <=
            { append(ops, {OP_LE, {tmp, pos, pos2}, {}, node->start, node->end}); break; }
            case 3: // >
            { append(ops, {OP_GT, {tmp, pos, pos2}, {}, node->start, node->end}); break; }
            case 4: // <
            { append(ops, {OP_LT, {tmp, pos, pos2}, {}, node->start, node->end}); break; }
            case 5: // =
            { append(ops, {OP_EQ, {tmp, pos, pos2}, {}, node->start, node->end}); break; }
        }

        freeTemp(ops, pos2);
        freeTemp(ops, pos);

        pos = tmp;
        t = ops;
        ops.clear();
        id += 2;
    }
    return {t, pos};
}

pair<vector<Operation>, int64_t> buildLogicOperation(BuildContext *ctx, Node *node)
{
    if (!is(node, "LogicOperation"))
    {
        return buildCompareOperation(ctx, node);
    }
    printf("Logic operation\n");
    vector<Operation> ops;
    auto [t, pos] = buildCompareOperation(ctx, node->nonTerm(0));
    HANDLE_NOT_NULL(pos, node->nonTerm(0));
    int64_t id = 1;
    while (node->nonTerm(id))
    {
        auto [t2, pos2] = buildCompareOperation(ctx, node->nonTerm(id + 1));
        HANDLE_NOT_NULL(pos2, node->nonTerm(id + 1));
        
        if (ctx->variables[pos]->provider != ctx->variables[pos2]->provider)
        {
            logError(ctx->filename, ctx->code, node->nonTerm(id + 0)->start, node->nonTerm(id + 0)->end, "Can't implictly compare %s and %s [providers differs]", printType(ctx->variables[pos]).c_str(), printType(ctx->variables[pos2]).c_str());
            return {{}, -1};
        }
        
        int64_t tmp = newTemp(ctx, getBaseType(ctx, "i32"));
        switch_var(node->nonTerm(id + 0))
        {
            case 0: // &&
            {
                append(ops, t);
                int64_t A_jmp = append(ops, {OP_JZ, {-1, pos}, {}, node->start, node->end});
                freeTemp(ops, pos);
                append(ops, t2);
                append(ops, {OP_NEW_INT, {tmp, -1}, {}, node->start, node->end});
                int64_t B_jmp = append(ops, {OP_JNZ, {-1, pos2}, {}, node->start, node->end});
                freeTemp(ops, pos2);
                int64_t pushFalse = append(ops, {OP_NEW_INT, {tmp, 0}, {}, node->start, node->end});

                ops[A_jmp].data[0] = pushFalse - A_jmp;
                ops[B_jmp].data[0] = ops.size() - B_jmp;
                break;
            }
            case 1: // ||
            {
                append(ops, t);
                int64_t A_jmp = append(ops, {OP_JNZ, {-1, pos}, {}, node->start, node->end});
                freeTemp(ops, pos);
                append(ops, t2);
                append(ops, {OP_NEW_INT, {tmp, 0}, {}, node->start, node->end});
                int64_t B_jmp = append(ops, {OP_JZ, {-1, pos2}, {}, node->start, node->end});
                freeTemp(ops, pos2);
                int64_t pushTrue = append(ops, {OP_NEW_INT, {tmp, -1}, {}, node->start, node->end});

                ops[A_jmp].data[0] = pushTrue - A_jmp;
                ops[B_jmp].data[0] = ops.size() - B_jmp;
                break;
            }
        }
        pos = tmp;
        t = ops;
        ops.clear();
        id += 2;
    }
    return {t, pos};
}


