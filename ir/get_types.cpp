#include "utils.hpp"
#include "../ir.hpp"


TypeContext *getDerivative(BuildContext *ctx, TypeContext *type, TypeContextType derivative, const string &provider)
{
    for (auto &val : ctx->types)
    {
        if (val->type == derivative && val->_vector.base == type && val->provider == provider)
        {
            return val;
        }
    }
    ctx->types.push_back(new TypeContext(8, derivative, provider, {._vector={type}}));
    return ctx->types.back();
}

TypeContext *getBaseType(BuildContext *ctx, const string &name, const string &provider)
{
    if (ctx->typeTable.contains({name, provider}))
    {
        TypeContext *type = ctx->typeTable[{name, provider}];
        if (IS_LINK_TYPE(type->type) && provider == "")
        {
            logError(ctx->filename, ctx->code, 0, 0, "Used link type without provider defined. [used %s]\n", name.c_str());
        }
        if (!IS_LINK_TYPE(type->type) && provider != "")
        {
            logError(ctx->filename, ctx->code, 0, 0, "Used scalar type [or struct] with provider defined, and this was found in type table. [used %s]\n", name.c_str());
        }
        return type;
    }
    if (ctx->typeTable.contains({name, ""}))
    {
        TypeContext *oldType = ctx->typeTable[{name, ""}], *newType;
        switch (oldType->type)
        {
            case TYPE_CLASS:
                newType = new TypeContext(oldType->size, oldType->type, provider, {._struct=oldType->_struct});
                break;
            case TYPE_SCALAR:
            case TYPE_UNION:
            case TYPE_RECORD:
                return oldType;
            case TYPE_ARRAY:
            case TYPE_PIPE:
            case TYPE_PROMISE:
                assert(false);
                return NULL;
        }
        newType->provider = provider;
        ctx->typeTable[{name, provider}] = newType;
        if (IS_LINK_TYPE(newType->type) && provider == "")
        {
            logError(ctx->filename, ctx->code, 0, 0, "Used link type without provider defined. [used %s]\n", name.c_str());
        }
        return newType;
    }
    return NULL;
}

TypeContext *getIntegerType(BuildContext *ctx, int64_t x)
{
    // TODO: how can i do this?
    // if (x >= INT8_MIN && x <= INT8_MAX) return getBaseType(ctx, "i8", provider);
    // if (x >= 0 && (uint64_t)x <= UINT8_MAX) return getBaseType(ctx, "u8", provider);
    // if (x >= INT16_MIN && x <= INT16_MAX) return getBaseType(ctx, "i16", provider);
    // if (x >= 0 && (uint64_t)x <= UINT16_MAX) return getBaseType(ctx, "u16", provider);
    if (x >= INT32_MIN && x <= INT32_MAX)  return getBaseType(ctx, "i32");
    if (x >= 0 && (uint64_t)x <= UINT32_MAX) return getBaseType(ctx, "u32");
    return getBaseType(ctx, "i64");
}

TypeContext *getType(BuildContext *ctx, Node *node)
{
    assert_type(node, "var_type");
    /* find base type */
    string baseName = Substr(ctx, node->nonTerm(0));
    TypeContext *cur = getBaseType(ctx, baseName, (node->variant == 0 ? Substr(ctx, node->nonTerm(1)) : ctx->provider));
    if (!cur)
    {
        logError(ctx->filename, ctx->code, node->start, node->end, "Unknown base type: %s\n", baseName.c_str());
        return NULL;
    }
    for (auto &child : node->childs)
    {
        if (is(child, "var_type_moditifer"))
        {
            cur = getDerivative(ctx, cur, vector<TypeContextType>{TYPE_ARRAY, TYPE_PIPE, TYPE_PROMISE}[child->variant % 3], 
                    (child->variant < 3 ? Substr(ctx, child->nonTerm(0)) : ctx->provider));
        }
    }
    return cur;
}


