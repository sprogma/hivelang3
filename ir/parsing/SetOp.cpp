#include "../../ir.hpp"
#include "../utils.hpp"


pair<vector<Operation>, int64_t> isPushToPipeOrPromise(BuildContext *ctx, Node *node, int64_t dataPos, TypeContext *dataType, TypeContext *type, auto buildFunc)
{
    vector<Operation> ops;
    
    if (type->type == TYPE_PIPE)
    {
        if (is_castable(type->_vector.base, dataType))
        {
            if (is_convertable(type->_vector.base, dataType) || ctx->enabled("implicit_castes"))
            {
                auto [target, targetPos] = buildFunc(ctx, node);
                HANDLE_NOT_NULL(targetPos, node);
                append(ops, target);
                append(ops, {OP_PUSH_PIPE, {targetPos, dataPos}, {}, node->start, node->end});
                freeTemp(ops, targetPos);
                return {ops, 0};
            }
            else
            {
                logError(ctx->filename, ctx->code, node->start, node->end, "can't automaticly cast this types, use <implicit_castes> flag to allow this cast: %s to %s", printType(dataType).c_str(), printType(type).c_str());
                return {{}, -1};
            }
        }
    }
    else if (type->type == TYPE_PROMISE)
    {
        if (is_castable(type->_vector.base, dataType))
        {
            if (is_convertable(type->_vector.base, dataType) || ctx->enabled("implicit_castes"))
            {
                auto [target, targetPos] = buildFunc(ctx, node);
                HANDLE_NOT_NULL(targetPos, node);
                append(ops, target);
                append(ops, {OP_PUSH_PROMISE, {targetPos, dataPos}, {}, node->start, node->end});
                freeTemp(ops, targetPos);
                return {ops, 0};
            }
            else
            {
                logError(ctx->filename, ctx->code, node->start, node->end, "can't automaticly cast this types, use <implicit_castes> flag to allow this cast: %s to %s", printType(dataType).c_str(), printType(type).c_str());
                return {{}, -1};
            }
        }
    }
    return {{}, -1};
}



pair<vector<Operation>, int64_t> buildSetToSimpleTerm(BuildContext *ctx, Node *node, int64_t dataPos, TypeContext *dataType)
{
    vector<Operation> ops;
    
    switch_var(node)
    {
        case 0: logError(ctx->filename, ctx->code, node->start, node->end, "Can't push to NEW operator"); break;
        case 1: logError(ctx->filename, ctx->code, node->start, node->end, "Can't push to integer constant"); break;
        case 2: logError(ctx->filename, ctx->code, node->start, node->end, "Can't push to float constant"); break;
        case 3:
        {
            /* push to variable/structure */
            if (ctx->names.find(Substr(ctx, node->nonTerm(0))) == ctx->names.end())
            {
                logError(ctx->filename, ctx->code, node->nonTerm(0)->start, node->nonTerm(0)->end, "Unknown variable name: %s", Substr(ctx, node->nonTerm(0)).c_str());
                break;
            }
            /* get variable */
            int64_t varId = ctx->names[Substr(ctx, node->nonTerm(0))];
            TypeContext *type = ctx->variables[varId];

            auto [offset, fldType] = GetFieldOffset(ctx, node, 1, type);

            /* check - if this is pushing of equal types */
            if (is_castable(type, dataType))
            {
                if (is_convertable(type, dataType) || ctx->enabled("implicit_castes"))
                {
                    if (type->type != TYPE_RECORD)
                    {
                        append(ops, {OP_MOV, {varId, dataPos}, {}, node->start, node->end});
                    }
                    else
                    {
                        append(ops, {OP_PUSH_VAR, {varId, offset, (int64_t)fldType, dataPos}, {}, node->start, node->end});
                    }
                    return {ops, -1};
                }
                else
                {
                    logError(ctx->filename, ctx->code, node->start, node->end, "can't automaticly cast this types, use <implicit_castes> flag to allow this cast: %s to %s", printType(dataType).c_str(), printType(type).c_str());
                    return {{}, -1};
                }
            }
            else
            {
                auto [xops, failed] = isPushToPipeOrPromise(ctx, node, dataPos, dataType, type, buildSimpleTerm);
                if (failed)
                {
                    logError(ctx->filename, ctx->code, node->start, node->end, "this types are uncastable - push is wrong: %s to %s", printType(dataType).c_str(), printType(type).c_str());
                    return {{}, -1};
                }
                return {xops, -1};
            }
        }
        case 4: 
            logError(ctx->filename, ctx->code, node->start, node->end, "Can't push to worker call");
            return {{}, -1};
    }
    return {{}, -1};
}


pair<vector<Operation>, int64_t> buildSetToExpression(BuildContext *ctx, Node *node, int64_t dataPos, TypeContext *dataType)
{
    vector<Operation> ops;
        
    /* get value - if it is pipe or it is promise - push to them */
    auto [code, pos] = buildExpression(ctx, node);
    HANDLE_NOT_NULL(pos, node);
    TypeContext *type = ctx->variables[pos];

    switch (type->type)
    {
        case TYPE_CLASS:
        case TYPE_UNION:
        case TYPE_RECORD:
        case TYPE_ARRAY:
        case TYPE_SCALAR:
            logError(ctx->filename, ctx->code, node->start, node->end, "can't push to expression [from braces] with value of type %s", printType(type).c_str());
            return {{}, -1};
        case TYPE_PIPE:
        case TYPE_PROMISE:
        {
            auto [xops, failed] = isPushToPipeOrPromise(ctx, node, dataPos, dataType, type, 
                [&](BuildContext *ctx, Node *node)->pair<vector<Operation>, int64_t>{(void)ctx;(void)node;return {code, pos};}
            );
            if (failed)
            {
                logError(ctx->filename, ctx->code, node->start, node->end, "this types are uncastable - push is wrong: %s to %s", printType(dataType).c_str(), printType(type).c_str());
                return {{}, -1};
            }
            return {xops, -1};
        }
    }
    return {{}, -1};
}

pair<vector<Operation>, int64_t> buildSetToIndex(BuildContext *ctx, Node *node, int64_t dataPos, TypeContext *dataType)
{    
    vector<Operation> ops;
    
    auto [array, arrayPos] = buildSimpleTerm(ctx, node->nonTerm(0));
    HANDLE_NOT_NULL(arrayPos, node->nonTerm(0));
    auto [index, indexPos] = buildExpression(ctx, node->nonTerm(1));
    HANDLE_NOT_NULL(indexPos, node->nonTerm(1));

    TypeContext *arrayType = ctx->variables[arrayPos];
    TypeContext *indexType = ctx->variables[indexPos];
    if (arrayType->type != TYPE_ARRAY)
    {
        logError(ctx->filename, ctx->code, node->start, node->end, "push to indexation not in array, wrong type: %s", printType(arrayType).c_str());
        return {{}, -1};
    }
    if (indexType->type != TYPE_SCALAR || (SCALAR_TYPE(indexType->_scalar.kind) != SCALAR_I && SCALAR_TYPE(indexType->_scalar.kind) != SCALAR_U))
    {
        logError(ctx->filename, ctx->code, node->start, node->end, "Index variable isn't integer scalar: %s", printType(indexType).c_str());
        return {{}, -1};
    }

    TypeContext *type = arrayType->_vector.base;

    /* find path to field */
    auto [offset, fldType] = GetFieldOffset(ctx, node, 2, type);

    /* check - if this is pushing of equal types */
    if (is_castable(fldType, dataType))
    {
        if (is_convertable(fldType, dataType) || ctx->enabled("implicit_castes"))
        {
            /* calculate arguments */
            append(ops, array);
            append(ops, index);

            append(ops, {OP_PUSH_ARRAY, {arrayPos, indexPos, offset, (int64_t)fldType, dataPos}, {}, node->start, node->end});

            freeTemp(ops, arrayPos);
            freeTemp(ops, indexPos);
            return {ops, -1};
        }
        else
        {
            logError(ctx->filename, ctx->code, node->start, node->end, "can't automaticly cast this types, use <implicit_castes> flag to allow this cast: %s to %s", printType(dataType).c_str(), printType(fldType).c_str());
            return {{}, -1};
        }
    }
    else
    {
        auto [xops, failed] = isPushToPipeOrPromise(ctx, node, dataPos, dataType, type, buildIndexOperation);
        if (failed)
        {
            logError(ctx->filename, ctx->code, node->start, node->end, "this types are uncastable - push is wrong: %s to %s", printType(dataType).c_str(), printType(type).c_str());
            return {{}, -1};
        }
        return {xops, -1};
    }
    return {{}, -1};
}


pair<vector<Operation>, int64_t> buildSetToQuery(BuildContext *ctx, Node *node, int64_t dataPos, TypeContext *dataType)
{
    vector<Operation> ops;
        
    if (node->variant == 0)
    {
        /* same as to expression */    
        auto [code, pos] = buildQueryOperation(ctx, node);
        HANDLE_NOT_NULL(pos, node);
        TypeContext *type = ctx->variables[pos];

        switch (type->type)
        {
            case TYPE_CLASS:
            case TYPE_UNION:
            case TYPE_RECORD:
            case TYPE_ARRAY:
            case TYPE_SCALAR:
                logError(ctx->filename, ctx->code, node->start, node->end, "can't push to query-expression [from braces] with value of type %s", printType(type).c_str());
                return {{}, -1};
            case TYPE_PIPE:
            case TYPE_PROMISE:
            {
                auto [xops, failed] = isPushToPipeOrPromise(ctx, node, dataPos, dataType, type, 
                    [&](BuildContext *ctx, Node *node)->pair<vector<Operation>, int64_t>{(void)ctx;(void)node;return {code, pos};}
                );
                if (failed)
                {
                    logError(ctx->filename, ctx->code, node->start, node->end, "this types are uncastable - push is wrong: %s to %s", printType(dataType).c_str(), printType(type).c_str());
                    return {{}, -1};
                }
                return {xops, -1};
            }
        }
        return {{}, -1};
    }
    else // infix query syntax - class field access
    {
        auto [code, position] = buildIndexOperation(ctx, node->nonTerm(0));
        HANDLE_NOT_NULL(position, node->nonTerm(0));
        TypeContext *type = ctx->variables[position];
        auto [attributes, attrCode] = getAttributeList(ctx, node->nonTerm(1), true);
        append(ops, attrCode);
        if (type->type != TYPE_CLASS)
        {
            logError(ctx->filename, ctx->code, node->nonTerm(0)->start, node->nonTerm(0)->end, "infix form of query must be used with classes, but used with %s", printType(type).c_str());
            return {{}, -1};
        }

        Node *path = node->nonTerm(2);
        if (!is(path, "SimpleTerm") || path->variant != 3)
        {
            logError(ctx->filename, ctx->code, path->start, path->end, "in infix form of class query, second part must be dotted identifer chain");
            return {{}, -1};
        }

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


        append(ops, code);
        append(ops, {OP_PUSH_CLASS, {position, offset, (int64_t)fldType, dataPos}, attributes, node->start, node->end});
        freeAttributeTemps(ops, attributes);
        freeTemp(ops, position);
        return {ops, -1};
    }
    return {{}, -1};
}


pair<vector<Operation>, int64_t> buildSetOperation(BuildContext *ctx, Node *node)
{
    if (!is(node, "SetOperation"))
    {
        return buildLogicOperation(ctx, node);
    }
    
    /* build rightmost part */
    auto [res, dataPos] = buildLogicOperation(ctx, node->childs.back());
    HANDLE_NOT_NULL(dataPos, node->childs.back());
    TypeContext *dataType = ctx->variables[dataPos];
    vector<Operation> ops;
    append(ops, res);
    
    /* set to each part in left */
    
    int id = 0;
    for (auto x : node->childs)
    {
        if (id + 1 == (int64_t)node->childs.size())
        {
            break;
        }
        id++;
        if (is(x, "SimpleTerm"))
        {
            auto [addOps, _] = buildSetToSimpleTerm(ctx, x, dataPos, dataType);
            append(ops, addOps);
        }
        else if (is(x, "expression"))
        {
            auto [addOps, _] = buildSetToExpression(ctx, x, dataPos, dataType);
            append(ops, addOps);
        }
        else if (is(x, "IndexOperation"))
        {
            auto [addOps, _] = buildSetToIndex(ctx, x, dataPos, dataType);
            append(ops, addOps);
        }
        else if (is(x, "QueryOperation"))
        {
            auto [addOps, _] = buildSetToQuery(ctx, x, dataPos, dataType);
            append(ops, addOps);
        
        }
    }
    return {ops, dataPos};
}

