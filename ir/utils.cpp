#include "utils.hpp"
#include "../ir.hpp"


string Substr(BuildContext *ctx, Node *node)
{
    return string(ctx->code + node->start, node->end - node->start);
}


int64_t newTemp(BuildContext *ctx, TypeContext *type)
{
    ctx->variables[ctx->nextTempId] = type;
    return ctx->nextTempId++;
}


void freeTemp(vector<Operation> &code, int64_t id)
{
    if (id >= FIRST_TEMP_ID)
    {
        code.push_back({OP_FREE_TEMP, {id}, {}, 0, 0});
    }
}


void freeAttributeTemps(vector<Operation> &ops, map<string, variant<string, int64_t>> attributes)
{
    for (auto &[key, value] : attributes) 
    {
        if (auto *intPtr = get_if<int64_t>(&value)) 
        {
            freeTemp(ops, *intPtr);
        }
    }
}


void append(vector<Operation> &a, vector<Operation> &b)
{
    a.insert(a.end(), b.begin(), b.end());
}


int64_t append(vector<Operation> &a, Operation b)
{
    a.push_back(b);
    return a.size() - 1;
}


bool validateProviderWithError(BuildContext *ctx, Node *node, const string &name)
{
    if (validateProvider(name))
    {
        return true;
    }
    logError(ctx->filename, ctx->code, node->start, node->end, "unknown provider: %s", name.c_str());
    return false;
}


bool handleNotNull(BuildContext *ctx, int64_t tmp, Node *node)
{
    if (tmp == -1)
    {
        logError(ctx->filename, ctx->code, node->start, node->end, "wrong usage of void expression");
        return true;
    }
    return false;
}


int64_t GetStringId(BuildContext *ctx, const vector<BYTE> &value)
{
    //TODO: find same string inside context
    ctx->result->strings.push_back(value);
    return ctx->result->strings.size() - 1;
}


WorkerDeclarationContext *getWorkerByName(BuildContext *ctx, string name)
{
    for (auto &[fn, key] : ctx->result->workers)
    {
        if (fn->name == name)
        {
            return fn;
        }
    }
    return NULL;
}


pair<int64_t, TypeContext *> GetFieldOffset(BuildContext *ctx, TypeContext *type, int64_t field)
{
    if (type->type == TYPE_RECORD || type->type == TYPE_CLASS)
    {
        (void)ctx;
        int64_t res = 0;
        for (auto &i : type->_struct.fields)
        {
            if (field-- == 0)
            {
                return {res, i};
            }
            res += i->size;
        }
        return {res, NULL};
    }
    else if (type->type == TYPE_UNION)
    {
        return {0, type->_struct.fields[field]};
    }
    return {0, NULL};
}

pair<int64_t, TypeContext *> GetFieldOffset(BuildContext *ctx, Node *node, int64_t fromId, TypeContext *type)
{
    int64_t id = fromId;
    int64_t offset = 0;
    while (node->nonTerm(id))
    {
        string field = Substr(ctx, node->nonTerm(id));
        if (type->type != TYPE_RECORD &&type->type != TYPE_RECORD && type->type != TYPE_UNION)
        {
            logError(ctx->filename, ctx->code, node->nonTerm(id)->start, node->nonTerm(id)->end, "Usage of dot on not structure/union object");
            break;
        }
        if (!type->_struct.names.contains(field))
        {
            logError(ctx->filename, ctx->code, node->nonTerm(id)->start, node->nonTerm(id)->end, "No field %s in structure", field.c_str());
            break;
        }
        tie(offset, type) = GetFieldOffset(ctx, type, type->_struct.names[field]);
        id++;
    }
    return {offset, type};
}

