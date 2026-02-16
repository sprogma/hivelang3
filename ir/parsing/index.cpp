#include "../../ir.hpp"
#include "../utils.hpp"


pair<vector<Operation>, int64_t> buildIndexOperation(BuildContext *ctx, Node *node)
{
    if (!is(node, "IndexOperation"))
    {
        return buildSimpleTerm(ctx, node);
    }
    printf("Index operation\n");

    auto [t, pos] = buildSimpleTerm(ctx, node->nonTerm(0));
    HANDLE_NOT_NULL(pos, node->nonTerm(0));
    auto [tIndex, posIndex] = buildExpression(ctx, node->nonTerm(1));
    HANDLE_NOT_NULL(posIndex, node->nonTerm(1));

    vector<Operation> ops;
    TypeContext *type = ctx->variables[pos];
    switch (type->type)
    {
        case TYPE_CLASS:
        case TYPE_UNION:
        case TYPE_RECORD:
        case TYPE_SCALAR:
        case TYPE_PIPE:
        case TYPE_PROMISE:
            logError(ctx->filename, ctx->code, node->start, node->end, "Can't use index on type %s", printType(type).c_str());
            return {{}, -1};
        case TYPE_ARRAY:
        {
            TypeContext *indexType = ctx->variables[posIndex];
            if (indexType->type != TYPE_SCALAR || (SCALAR_TYPE(indexType->_scalar.kind) != SCALAR_I && SCALAR_TYPE(indexType->_scalar.kind) != SCALAR_U))
            {
                logError(ctx->filename, ctx->code, node->start, node->end, "Index variable isn't integer scalar: %s", printType(type).c_str());
                return {{}, -1};
            }
            append(ops, t);
            append(ops, tIndex);
            /* generate path */
            auto [offset, fldType] = GetFieldOffset(ctx, node, 2, type->_vector.base);
            int64_t tmp = newTemp(ctx, fldType);
            append(ops, {OP_QUERY_INDEX, {tmp, pos, offset, (int64_t)fldType, posIndex}, {}, node->start, node->end});
            freeTemp(ops, pos);
            freeTemp(ops, posIndex);
            return {ops, tmp};
        }
    }
}

