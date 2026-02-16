#include "../../ir.hpp"
#include "../utils.hpp"


pair<vector<Operation>, int64_t> buildNewObject(BuildContext *ctx, Node *node, TypeContext *type, const map<string, variant<string, int64_t>> &attributes)
{
    vector<Operation> ops;

    int64_t resultPos = newTemp(ctx, type);
    
    vector<int> args;
    for (auto &ch : node->nonTerm(2)->childs)
    {
        if (is(ch, "expression"))
        {
            auto [code, pos] = buildExpression(ctx, ch);
            HANDLE_NOT_NULL(pos, ch);
            append(ops, code);
            args.push_back(pos);
        }
    }

    switch (type->type)
    {
        case TYPE_UNION:
        case TYPE_RECORD:
        case TYPE_SCALAR:
            logError(ctx->filename, ctx->code, node->start, node->end, "can't use NEW expression with union/record/scalar types");
            return {{}, -1};
        case TYPE_CLASS:
        {
            if (args.size() > 0)
            {
                logError(ctx->filename, ctx->code, node->start, node->end, "NEW expression with class doesn't support field initialization for now");
                return {{}, -1};
            }
            append(ops, {OP_NEW_CLASS, {resultPos}, attributes, node->start, node->end});
            freeAttributeTemps(ops, attributes);
            return {ops, resultPos};
        }
        case TYPE_ARRAY:
        {
            if (args.size() != 1)
            {
                logError(ctx->filename, ctx->code, node->start, node->end, "NEW expression with array need 1 parameter - length");
                return {{}, -1};
            }
            append(ops, {OP_NEW_ARRAY, {resultPos, args[0]}, attributes, node->start, node->end});
            freeAttributeTemps(ops, attributes);
            return {ops, resultPos};
        }
        case TYPE_PIPE:
        {
            if (args.size() > 0)
            {
                logError(ctx->filename, ctx->code, node->start, node->end, "NEW expression with pipe can't take any parameters");
                return {{}, -1};
            }
            append(ops, {OP_NEW_PIPE, {resultPos}, attributes, node->start, node->end});
            freeAttributeTemps(ops, attributes);
            return {ops, resultPos};
        }
        case TYPE_PROMISE:
        {
            if (args.size() > 0)
            {
                logError(ctx->filename, ctx->code, node->start, node->end, "NEW expression with promise can't take any parameters");
                return {{}, -1};
            }
            append(ops, {OP_NEW_PROMISE, {resultPos}, attributes, node->start, node->end});
            freeAttributeTemps(ops, attributes);
            return {ops, resultPos};
        }
    }
    return {{}, -1};
}



pair<vector<Operation>, int64_t> buildNewString(BuildContext *ctx, Node *node, TypeContext *type, const map<string, variant<string, int64_t>> &attributes)
{
    vector<Operation> ops;

    int64_t resultPos = newTemp(ctx, type);
            
    if (type->type != TYPE_ARRAY)
    {
        logError(ctx->filename, ctx->code, node->start, node->end, "NEW expression string variant must be used only with arrays");
        return {{}, -1};
    }

    TypeContext *element = type->_vector.base;
    if (element->type != TYPE_SCALAR || (
        SCALAR_TYPE(element->_scalar.kind) != SCALAR_U &&
        SCALAR_TYPE(element->_scalar.kind) != SCALAR_I ))
    {
        logError(ctx->filename, ctx->code, node->start, node->end, "NEW expression string element must be integer scalar");
        return {{}, -1};
    }

    // get value
    string value = Substr(ctx, node->nonTerm(2));
    vector<BYTE> content((value.size() - 1) * element->size, 0); // - 2 [""] + 1 terminating zero
    
    // TODO: utf8->16/32 conversion and better escaping
    int64_t used_len = 0;
    for (int64_t i = 1; i + 1 < (int64_t)value.size(); ++i)
    {
        if ((value[i] & 0x80) && element->size != 1)
        {
            logError(ctx->filename, ctx->code, node->nonTerm(2)->start, node->nonTerm(2)->end, "string literals doen't support UTF8->UTF16/32 encoding conversion for now");
            return {{}, -1};
        }
        if (value[i] == '\\')
        {
            switch (value[i + 1])
            {
                case '\\': content[element->size * (used_len++)] = '\\'; break;
                case '\"': content[element->size * (used_len++)] = '\"'; break;
                case '\'': content[element->size * (used_len++)] = '\''; break;
                case 'n':  content[element->size * (used_len++)] = '\n'; break;
                case 't':  content[element->size * (used_len++)] = '\t'; break;
                case 'v':  content[element->size * (used_len++)] = '\v'; break;
                case 'e':  content[element->size * (used_len++)] = '\e'; break;
                case 'a':  content[element->size * (used_len++)] = '\a'; break;
                case 'r':  content[element->size * (used_len++)] = '\r'; break;
                default:
                    logError(ctx->filename, ctx->code, node->nonTerm(2)->start, node->nonTerm(2)->end, "unknown string literal escaped symbol: %c", value[i + 1]);
            }
            i++;
        }
        else
        {
            content[element->size * (used_len++)] = value[i];
        }
    }
    content.resize((used_len + 1) * element->size);
    int64_t vid = GetStringId(ctx, content);
    append(ops, {OP_NEW_STRING, {resultPos, vid}, attributes, node->start, node->end});
    freeAttributeTemps(ops, attributes);
    return {ops, resultPos};
}



pair<vector<Operation>, int64_t> buildNewOperator(BuildContext *ctx, Node *node)
{
    assert_type(node, "new_operator"); 

    vector<Operation> ops;

    
    auto [attributes, attrCode] = getAttributeList(ctx, node->nonTerm(1), true);
    append(ops, attrCode);
    
    TypeContext *type = getType(ctx, node->nonTerm(0));
    if (type == NULL)
    {
        return {{}, -1};
    }
    
    if (node->variant == 0)
    {
        return buildNewObject(ctx, node, type, attributes);
    }
    else
    {
        return buildNewString(ctx, node, type, attributes);
    }
}


pair<vector<Operation>, int64_t> buildInteger(BuildContext *ctx, Node *node)
{
    vector<Operation> ops;
    
    char *end;
    int64_t intValue = strtoll(Substr(ctx, node->nonTerm(0)).c_str(), &end, 0), tmp;
    if (ctx->current->attributes.contains("integer64"))
    {
        tmp = newTemp(ctx, getBaseType(ctx, "i64"));
    }
    else
    {
        tmp = newTemp(ctx, getIntegerType(ctx, intValue));
    }
    append(ops, {OP_NEW_INT, {tmp, intValue}, {}, node->start, node->end});
    return {ops, tmp};
}


pair<vector<Operation>, int64_t> buildFloat(BuildContext *ctx, Node *node)
{
    vector<Operation> ops;
    char *end;
    double fltValue = strtod(Substr(ctx, node->nonTerm(0)).c_str(), &end);
    int64_t intValue = *(int64_t *)&fltValue;
    int64_t tmp = newTemp(ctx, getBaseType(ctx, "f64"));
    append(ops, {OP_NEW_FLOAT, {tmp, intValue}, {}, node->start, node->end});
    return {ops, tmp};
}


pair<vector<Operation>, int64_t> buildQueryVariable(BuildContext *ctx, Node *node)
{
    vector<Operation> ops;
    
    /* load variable */
    string name = Substr(ctx, node->nonTerm(0));
    if (!ctx->names.contains(name))
    {
        logError(ctx->filename, ctx->code, node->start, node->end, "usage of unknown variable: %s", name.c_str());
        return {{}, -1};
    }
    int64_t varId = ctx->names[name];
    TypeContext *type = ctx->variables[varId];

    auto [offset, fldType] = GetFieldOffset(ctx, node, 1, type);

    if (ctx->enabled("optimize_query_var") && type->type != TYPE_RECORD)
    {
        return {ops, varId};
    }

    int64_t tmp = newTemp(ctx, type);
    if (type->type != TYPE_RECORD)
    {
        append(ops, {OP_MOV, {tmp, varId}, {}, node->start, node->end});
    }
    else
    {
        append(ops, {OP_QUERY_VAR, {tmp, varId, offset, (int64_t)fldType}, {}, node->start, node->end});
    }
    return {ops, tmp};
}


pair<vector<Operation>, int64_t> buildFunctionInputArgumentList(BuildContext *ctx, WorkerDeclarationContext *fn, Node *node, vector<int64_t> &args, vector<int64_t> &freeList)
{ 
    vector<Operation> ops;
    
    int64_t ind = 0;
    for (; ind < (int64_t)fn->inputs.size(); ++ind)
    {
        Node *ch = node->nonTerm(ind);
        if (ch == NULL)
        {
            logError(ctx->filename, ctx->code, node->start, node->end, "Too little parameters [%lld/%lld]\n", ind, fn->inputs.size());
            return {{}, -1};
        }
        assert_type(ch, "expression");
        auto [res, pos] = buildExpression(ctx, ch);
        HANDLE_NOT_NULL(pos, ch);
        append(ops, res);
        args.push_back(pos);
        freeList.push_back(pos);
    }
    

    if (node->nonTerm(ind))
    {
        logError(ctx->filename, ctx->code, node->nonTerm(ind)->start, node->nonTerm(ind)->end, "Too many parameters");
        return {{}, -1};
    }
    
    return {ops, -1};
}

pair<vector<Operation>, int64_t> buildFunctionOutputArgumentList(BuildContext *ctx, WorkerDeclarationContext *fn, Node *node, vector<int64_t> &args, vector<int64_t> &outputs)
{
    vector<Operation> ops;
    int64_t tmpResult = -1, outId = 0;
    for (auto ch : node->childs)
    {
        if (is(ch, "result_list_identifer"))
        {
            if (outId >= (int64_t)fn->outputs.size())
            {
                logError(ctx->filename, ctx->code, ch->start, ch->end, "Too many output arguments for worker");
                return {{}, -1};
            }
            switch_var(ch)
            {
                case 0: /* "star" output - this is return type */
                {
                    if (tmpResult != -1)
                    {
                        logError(ctx->filename, ctx->code, ch->start, ch->end, "Two or more * in outputs of call - expression can have only one value");
                        return {{}, -1};
                    }
                    tmpResult = newTemp(ctx, fn->outputs[outId].second);
                    args.push_back(tmpResult);
                    outputs.push_back(-1);
                    break;
                }
                case 1: /* normal output */
                {
                    string varName = Substr(ctx, ch->nonTerm(0));
                    int64_t varId = ctx->nextVarId++;
                    ctx->variables[varId] = fn->outputs[outId].second;
                    ctx->names[varName] = varId;
                    args.push_back(varId);
                    outputs.push_back(varId);
                    break;
                }
            }
            outId++;
        }
    }

    if (outputs.size() > fn->outputs.size())
    {
        logError(ctx->filename, ctx->code, node->start, node->end, "Too many outputs [%lld/%lld]", outputs.size(), fn->outputs.size());
        return {{}, -1};
    }
    if (outputs.size() > fn->outputs.size())
    {
        logError(ctx->filename, ctx->code, node->start, node->end, "Too little outputs [%lld/%lld]", outputs.size(), fn->outputs.size());
        return {{}, -1};
    }
    
    return {ops, tmpResult};
}

pair<vector<Operation>, int64_t> buildFunctionCall(BuildContext *ctx, Node *node)
{
    vector<Operation> ops;

    assert_type(node, "function_call");
    
    Node *argList = node->nonTerm(0);
    Node *nameNode = node->nonTerm(1);
    string name = Substr(ctx, nameNode->nonTerm(0));
    string provider = (nameNode->variant == 0 ? Substr(ctx, nameNode->nonTerm(1)) : ctx->provider);
    auto [attributes, attrCode] = getAttributeList(ctx, node->nonTerm(2), true);
    append(ops, attrCode);
    Node *outList = node->nonTerm(3);

    /* build all arguments */
    vector<int64_t> freeList;
    vector<int64_t> args;
    vector<int64_t> outputs;

    /* load function by name */
    WorkerDeclarationContext *fn = getWorkerByName(ctx, name);
    if (fn == NULL)
    {
        logError(ctx->filename, ctx->code, node->start, node->end, "can't find worker named %s", name.c_str());
        return {{}, -1};
    }
    printf("Call of %s...\n", name.c_str());


    args.push_back(ctx->result->workers[fn]);


    auto [argOps, _] = buildFunctionInputArgumentList(ctx, fn, argList, args, freeList);
    append(ops, argOps);

    /* declare output variables */
    auto [outOps, tmpResult] = buildFunctionOutputArgumentList(ctx, fn, outList, args, outputs);
    append(ops, outOps);
    
    /* create all outputs */
    for (auto i : outputs)
    {
        if (i == -1)
        {
            i = tmpResult;
        }
        if (ctx->variables[i]->type == TYPE_PIPE)
        {
            append(ops, {OP_NEW_PIPE, {i}, {}, outList->start, outList->end});
        }
        else if (ctx->variables[i]->type == TYPE_PROMISE)
        {
            append(ops, {OP_NEW_PROMISE, {i}, {}, outList->start, outList->end});
        }
        else
        {
            logError(ctx->filename, ctx->code, outList->start, outList->end, "Return types can be only pipe/promise, have %s", printType(ctx->variables[i]).c_str());
        }
    }

    /* create operation */
    attributes["provider"] = provider;
    fn->used_providers.insert(provider);
    ctx->result->used_providers.insert(provider);
    // expand "on" attribute
    if (!attributes.contains("on") && fn->attributes.contains("on"))
    {
        attributes["on"] = fn->attributes["on"];
    }
    append(ops, {OP_CALL, args, attributes, node->start, node->end});
    freeAttributeTemps(ops, attributes);

    /* free all inputs */
    for (auto &i : freeList)
    {
        freeTemp(ops, i);
    }

    return {ops, tmpResult};
}


pair<vector<Operation>, int64_t> buildSimpleTerm(BuildContext *ctx, Node *node)
{
    if (!is(node, "SimpleTerm"))
    {
        return buildExpression(ctx, node);
    }
    printf("Simple Term operation\n");

    switch_var(node)
    {
        case 0: // new operator
        {
            Node *newOp = node->nonTerm(0);
            return buildNewOperator(ctx, newOp);
        }
        case 1: // integer
        {
            return buildInteger(ctx, node);
        }
        case 2: // float
        {
            return buildFloat(ctx, node);
        }
        case 3: // identifer.identifer.identifer...
        {
            return buildQueryVariable(ctx, node);
        }
        case 4: // fn call...
        {
            Node *fnCall = node->nonTerm(0);
            return buildFunctionCall(ctx, fnCall);
        }
    }
    return {{}, -1};
}

