#include "../../ir.hpp"
#include "../utils.hpp"


pair<vector<Operation>, int64_t> buildPrefixQuery(BuildContext *ctx, Node *node)
{
    vector<Operation> ops;
    auto [t, pos] = buildQueryOperation(ctx, node->nonTerm(1));
    HANDLE_NOT_NULL(pos, node->nonTerm(1));
    append(ops, t);
    TypeContext *elemType = ctx->variables[pos];
    switch (elemType->type)
    {
        case TYPE_CLASS:
        case TYPE_UNION:
        case TYPE_RECORD:
        case TYPE_SCALAR:
            logError(ctx->filename, ctx->code, node->start, node->end, "Can't use prefix query on type %s", printType(elemType).c_str());
            return {{}, -1};
        case TYPE_ARRAY:
        {
            int64_t tmp = newTemp(ctx, getBaseType(ctx, "i64"));
            append(ops, {OP_QUERY_ARRAY, {tmp, pos}, {}, node->start, node->end});
            freeTemp(ops, pos);
            return {ops, tmp};
        }
        case TYPE_PIPE:
        {
            int64_t tmp = newTemp(ctx, elemType->_vector.base);
            append(ops, {OP_QUERY_PIPE, {tmp, pos}, {}, node->start, node->end});
            freeTemp(ops, pos);
            return {ops, tmp};
        }
        case TYPE_PROMISE:
        {
            int64_t tmp = newTemp(ctx, elemType->_vector.base);
            append(ops, {OP_QUERY_PROMISE, {tmp, pos}, {}, node->start, node->end});
            freeTemp(ops, pos);
            return {ops, tmp};
        }
    }
    return {{}, -1};
}

pair<vector<Operation>, int64_t> buildInfixQuery(BuildContext *ctx, Node *node)
{
    vector<Operation> ops;
    auto [code, position] = buildIndexOperation(ctx, node->nonTerm(0));
    HANDLE_NOT_NULL(position, node->nonTerm(0));
    TypeContext *type = ctx->variables[position];
    auto [attributes, attrCode] = getAttributeList(ctx, node->nonTerm(1), true);
    append(ops, attrCode);
    if (type->type != TYPE_CLASS)
    {
        logError(ctx->filename, ctx->code, node->nonTerm(0)->start, node->nonTerm(0)->end, "infix form of query must be used with classes, but used with: %s", printType(type).c_str());
        return {{}, -1};
    }
    if (!is(node->nonTerm(2), "SimpleTerm") || node->nonTerm(2)->variant != 3)
    {
        logError(ctx->filename, ctx->code, node->nonTerm(2)->start, node->nonTerm(2)->end, "In infix form of class query, second part must be dotted identifer chain");
        return {{}, -1};
    }
    Node *path = node->nonTerm(2);
    /* get first field manually */
    Node *first = path->nonTerm(0);
    if (first == NULL)
    {
        logError(ctx->filename, ctx->code, path->start, path->end, "In infix form of class query, no path to field");
        return {{}, -1};
    }
    string fname = Substr(ctx, first);
    if (!type->_struct.names.contains(fname))
    {
        logError(ctx->filename, ctx->code, first->start, first->end, "In infix form of class query, no field named %s", fname.c_str());
        return {{}, -1};
    }
    TypeContext *pptype = type;
    type = type->_struct.fields[type->_struct.names[fname]];
    /* parse path to final field */
    auto [offset, fldType] = GetFieldOffset(ctx, path, 1, type);
    offset += GetFieldOffset(ctx, pptype, pptype->_struct.names[fname]).first;
    int64_t tmp = newTemp(ctx, fldType);
    append(ops, code);
    append(ops, {OP_QUERY_CLASS, {tmp, position, offset, (int64_t)fldType}, attributes, node->start, node->end});
    freeAttributeTemps(ops, attributes);
    return {ops, tmp};
}


pair<vector<Operation>, int64_t> buildQueryOperation(BuildContext *ctx, Node *node)
{
    if (!is(node, "QueryOperation"))
    {
        return buildIndexOperation(ctx, node);
    }
    printf("Query operation\n");
    switch_var(node)
    {
        case 0: // prefix query
        {
            return buildPrefixQuery(ctx, node);
        }
        case 1: // infix query
        {
            return buildInfixQuery(ctx, node);
        }
    }
    return {{}, -1};
}


