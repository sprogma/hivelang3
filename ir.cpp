#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <map>
#include <string>
#include <vector>
#include <ranges>
#include <variant>
#include <algorithm>

using namespace std;

#include "optimization/optimizer.hpp"
#include "codegen/codegen.hpp"
#include "logger.hpp"
#include "ast.hpp"
#include "ir.hpp"

#define is(node, str) (node->rule && strcmp(node->rule->name, str) == 0)

#define assert_type(node, str) do{if (!is((node), str)){if((node)->rule){printf("[Have <%s> instead]\n", (node)->rule->name);}assert(is((node), str));}}while(0)
#define switch_type(x) switch ((x)->rule ? (x)->rule->id : 0)
#define switch_var(x) switch ((x)->variant)



/* pre-intermediate representation */
struct Operation
{
    OperationType type;
    vector<int64_t> data;
    map<string, variant<string, int64_t>> attributes = {};

    int64_t code_start, code_end;
};


pair<vector<Operation>, int64_t> buildExpression(BuildContext *ctx, Node *node);
vector<Operation> buildCodeBlock(BuildContext *ctx, Node *node);
void freeTemp(vector<Operation> &code, int64_t id);

void append(vector<Operation> &a, vector<Operation> &b)
{
    a.insert(a.end(), b.begin(), b.end());
}

int64_t append(vector<Operation> &a, Operation b)
{
    a.push_back(b);
    return a.size() - 1;
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

bool validateProviderWithError(BuildContext *ctx, Node *node, const string &name)
{
    if (validateProvider(name))
    {
        return true;
    }
    logError(ctx->filename, ctx->code, node->start, node->end, "unknown provider: %s", name.c_str());
    return false;
}

string Substr(BuildContext *ctx, Node *node)
{
    return string(ctx->code + node->start, node->end - node->start);
}

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

#define HANDLE_NOT_NULL(tmp, node) \
    if (handleNotNull(ctx, tmp, node)) return {{}, -1};

bool handleNotNull(BuildContext *ctx, int64_t tmp, Node *node)
{
    if (tmp == -1)
    {
        logError(ctx->filename, ctx->code, node->start, node->end, "wrong usage of void expression");
        return true;
    }
    return false;
}

// from t2 to t1
bool is_convertable(TypeContext *t1, TypeContext *t2)
{
    if (t1 == t2)
    {
        return true;
    }
    if (t1->type == TYPE_SCALAR && t2->type == TYPE_SCALAR)
    {
        if (t1->_scalar.kind & SCALAR_I)
        {
            if (t2->_scalar.kind & SCALAR_I) return t1->size >= t2->size;
            if (t2->_scalar.kind & SCALAR_U) return t1->size > t2->size;
            if (t2->_scalar.kind & SCALAR_F) return false;
        }
        else if (t1->_scalar.kind & SCALAR_U)
        {
            if (t2->_scalar.kind & SCALAR_I) return false;
            if (t2->_scalar.kind & SCALAR_U) return t1->size >= t2->size;
            if (t2->_scalar.kind & SCALAR_F) return false;
        }
        else if (t1->_scalar.kind & SCALAR_F)
        {
            if (t2->_scalar.kind & SCALAR_I) return (t1->size == 8 && t2->size <= 4);
            if (t2->_scalar.kind & SCALAR_U) return (t1->size == 8 && t2->size <= 4);
            if (t2->_scalar.kind & SCALAR_F) return t1->size >= t2->size;
        }
    }
    return false;
}

// from t2 to t1
bool is_castable(TypeContext *t1, TypeContext *t2)
{
    if (t1 == t2)
    {
        return true;
    }
    if (t1->type == TYPE_SCALAR && t2->type == TYPE_SCALAR)
    {
        return true; // all known scalars has casing rules
    }
    if (t1->type == TYPE_CLASS && t2->type == TYPE_CLASS)
    {
        // TODO: make something more clever
        return true; // all classes can be castable too
    }
    if (t1->type == t2->type && t1->size == t2->size)
    {
        // TODO: move check to providers instead of confirming all
        switch (t1->type)
        {
            case TYPE_CLASS:
            case TYPE_UNION:
            case TYPE_RECORD:
                return t1->_struct.fields == t2->_struct.fields &&
                       t1->_struct.names == t2->_struct.names;
            case TYPE_SCALAR:
                return t1->_scalar.kind == t2->_scalar.kind;
            case TYPE_ARRAY:
            case TYPE_PIPE:
            case TYPE_PROMISE:
                return t1->_vector.base == t2->_vector.base;
        }
    }
    return false;
}

pair<bool, TypeContext *> operation_types(TypeContext *t1, TypeContext *t2)
{
    if (t1 == t2) { return {true, t1}; }
    if (t1->type == TYPE_SCALAR && t2->type == TYPE_SCALAR)
    {
        if (SCALAR_TYPE(t1->_scalar.kind) == SCALAR_TYPE(t2->_scalar.kind))
        {
            return {true, (t1->size > t2->size ? t1 : t2)};
        }
        // TODO: more precision casting
        return {false, NULL};
    }
    return {false, NULL};
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

TypeContext *getBaseType(BuildContext *ctx, const string &name, const string &provider="")
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
                __builtin_unreachable();
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
    TypeContext *cur = getBaseType(ctx, baseName, (node->variant == 1 ? Substr(ctx, node->nonTerm(1)) : ctx->provider));
    if (!cur)
    {
        logError(ctx->filename, ctx->code, node->start, node->end, "Unknown base type: %s\n", baseName.c_str());
        return NULL;
    }
    for (auto &child : node->childs)
    {
        if (is(child, "var_type_moditifer"))
        {
            if (child->variant < 3)
            {
                cur = getDerivative(ctx, cur, vector<TypeContextType>{TYPE_ARRAY, TYPE_PIPE, TYPE_PROMISE}[child->variant % 3], Substr(ctx, child->nonTerm(0)));
            }
            else
            {
                cur = getDerivative(ctx, cur, vector<TypeContextType>{TYPE_ARRAY, TYPE_PIPE, TYPE_PROMISE}[child->variant % 3], ctx->provider);
            }
        }
    }
    return cur;
}

pair<map<string, variant<string, int64_t>>, vector<Operation>> getAttributeList(BuildContext *ctx, Node *node, bool support_expression)
{
    map<string, variant<string, int64_t>> attrs;
    vector<Operation> ops;
    int64_t attrId = 0;
    assert_type(node, "attribute_list");
    while (node->nonTerm(attrId))
    {
        Node *attr = node->nonTerm(attrId);
        switch_var(attr)
        {
            case 0: 
            {
                if (!support_expression)
                {
                    logError(ctx->filename, ctx->code, attr->start, attr->end, "Can't use expression as attribute here.");
                    break;
                }
                // key expression pair - build expression, store temp_id as string [8 bytes]
                Node *key = attr->nonTerm(0);
                Node *value = attr->nonTerm(1);
                assert_type(key, "identifer_with_dots");
                assert_type(value, "expression");
                auto [expOps, tmp_id] = buildExpression(ctx, value);
                append(ops, expOps);
                attrs[Substr(ctx, key)] = tmp_id;
                break;
            }
            case 1:
            {
                // simple key value pair
                Node *key = attr->nonTerm(0);
                Node *value = attr->nonTerm(1);
                assert_type(key, "identifer_with_dots");
                assert_type(value, "identifer_or_number");
                attrs[Substr(ctx, key)] = Substr(ctx, value);
                break;
            }
        }
        attrId++;
    }
    return {attrs, ops};
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


void processStructure(BuildContext *ctx, TypeContextType type, Node *node)
{
    switch (type)
    {
        case TYPE_RECORD: assert_type(node, "_record"); break;
        case TYPE_UNION: assert_type(node, "_union"); break;
        case TYPE_CLASS: assert_type(node, "_class"); break;
        default:
            logError(ctx->filename, ctx->code, node->start, node->end, "processStructure called with not TYPE_RECORD/TYPE_UNION/TYPE_CLASS");
            return;
    }
    /* register new type */
    string name = Substr(ctx, node->nonTerm(0));
    int64_t total_size = 0;
    vector<TypeContext *> fields;
    map<string, int64_t> names;
    for (auto &child : node->childs)
    {
        if (is(child, "var_declaration"))
        {
            TypeContext *type = getType(ctx, child->nonTerm(0));
            if (type != NULL)
            {
                for (auto name : child->childs)
                {
                    if (is(name, "identifer"))
                    {
                        total_size += type->size;
                        names[Substr(ctx, name)] = fields.size();
                        fields.push_back(type);
                    }
                }
            }
        }
    }
    if (type == TYPE_CLASS)
    {
        total_size = 8;
    }
    printf("Type %s generated\n", name.data());
    for (auto &[k, v] : names)
    {
        printf("Field %s -> type %p\n", k.data(), fields[v]);
    }
    ctx->types.push_back(new TypeContext(total_size, type, "", {._struct={fields, names}}));
    ctx->typeTable[{name, ""}] = ctx->types.back();
}

vector<pair<string, TypeContext *>> readWorkerArgList(BuildContext *ctx, Node *node)
{
    assert_type(node, "arguments_list");
    vector<pair<string, TypeContext *>> res;

    int64_t id = 0;
    while (node->nonTerm(id))
    {
        TypeContext *type = getType(ctx, node->nonTerm(id + 0));
        if (type != NULL)
        {
            string name = Substr(ctx, node->nonTerm(id + 1));
            printf("Input %s of type %p\n", name.data(), type);
            res.push_back({name, type});
        }
        id += 2;
    }

    return res;
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

pair<vector<Operation>, int64_t> buildSimpleTerm(BuildContext *ctx, Node *node)
{
    if (!is(node, "SimpleTerm"))
    {
        return buildExpression(ctx, node);
    }
    printf("Simple Term operation\n");

    vector<Operation> ops;

    switch_var(node)
    {
        case 0: // new operator
        {
            Node *newOp = node->nonTerm(0);

            assert_type(newOp, "new_operator");

            if (newOp->variant == 0)
            {
                TypeContext *type = getType(ctx, newOp->nonTerm(0));
                if (type == NULL)
                {
                    return {{}, -1};
                }
                
                auto [attributes, attrCode] = getAttributeList(ctx, newOp->nonTerm(1), true);
                append(ops, attrCode);

                vector<int> args;
                for (auto &ch : newOp->nonTerm(2)->childs)
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
                        logError(ctx->filename, ctx->code, newOp->start, newOp->end, "can't use NEW expression with union/record/scalar types");
                        return {{}, -1};
                    case TYPE_CLASS:
                    {
                        if (args.size() > 0)
                        {
                            logError(ctx->filename, ctx->code, newOp->start, newOp->end, "NEW expression with class doesn't support field initialization for now");
                            return {{}, -1};
                        }
                        int64_t resultPos = newTemp(ctx, type);
                        append(ops, {OP_NEW_CLASS, {resultPos}, attributes, node->start, node->end});
                        freeAttributeTemps(ops, attributes);
                        return {ops, resultPos};
                    }
                    case TYPE_ARRAY:
                    {
                        if (args.size() != 1)
                        {
                            logError(ctx->filename, ctx->code, newOp->start, newOp->end, "NEW expression with array need 1 parameter - length");
                            return {{}, -1};
                        }
                        int64_t resultPos = newTemp(ctx, type);
                        append(ops, {OP_NEW_ARRAY, {resultPos, args[0]}, attributes, node->start, node->end});
                        freeAttributeTemps(ops, attributes);
                        return {ops, resultPos};
                    }
                    case TYPE_PIPE:
                    {
                        if (args.size() > 0)
                        {
                            logError(ctx->filename, ctx->code, newOp->start, newOp->end, "NEW expression with pipe can't take any parameters");
                            return {{}, -1};
                        }
                        int64_t resultPos = newTemp(ctx, type);
                        append(ops, {OP_NEW_PIPE, {resultPos}, attributes, node->start, node->end});
                        freeAttributeTemps(ops, attributes);
                        return {ops, resultPos};
                    }
                    case TYPE_PROMISE:
                    {
                        if (args.size() > 0)
                        {
                            logError(ctx->filename, ctx->code, newOp->start, newOp->end, "NEW expression with promise can't take any parameters");
                            return {{}, -1};
                        }
                        int64_t resultPos = newTemp(ctx, type);
                        append(ops, {OP_NEW_PROMISE, {resultPos}, attributes, node->start, node->end});
                        freeAttributeTemps(ops, attributes);
                        return {ops, resultPos};
                    }
                }
                return {{}, -1};
            }
            else
            {
                // this is new string
                auto [attributes, attrCode] = getAttributeList(ctx, newOp->nonTerm(1), true);
                append(ops, attrCode);
                // get type
                TypeContext *type = getType(ctx, newOp->nonTerm(0));
                if (type == NULL)
                {
                    return {{}, -1};
                }
                if (type->type != TYPE_ARRAY)
                {
                    logError(ctx->filename, ctx->code, newOp->start, newOp->end, "NEW expression string variant must be used only with arrays");
                    return {{}, -1};
                }
                TypeContext *element = type->_vector.base;
                if (element->type != TYPE_SCALAR || (
                    SCALAR_TYPE(element->_scalar.kind) != SCALAR_U &&
                    SCALAR_TYPE(element->_scalar.kind) != SCALAR_I
                ))
                {
                    logError(ctx->filename, ctx->code, newOp->start, newOp->end, "NEW expression string element must be integer scalar");
                    return {{}, -1};
                }
                // get value
                string value = Substr(ctx, newOp->nonTerm(2));
                vector<BYTE> content((value.size() - 1) * element->size, 0); // - 2 [""] + 1 terminating zero
                // TODO: utf8->16/32 conversion
                int64_t used_len = 0;
                for (int64_t i = 1; i + 1 < (int64_t)value.size(); ++i)
                {
                    if ((value[i] & 0x80) && element->size != 1)
                    {
                        logError(ctx->filename, ctx->code, newOp->nonTerm(2)->start, newOp->nonTerm(2)->end, "string literals doen't support UTF8->UTF16/32 encoding conversion for now");
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
                                logError(ctx->filename, ctx->code, newOp->nonTerm(2)->start, newOp->nonTerm(2)->end, "unknown string literal escaped symbol: %c", value[i + 1]);
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
                int64_t resultPos = newTemp(ctx, type);
                append(ops, {OP_NEW_STRING, {resultPos, vid}, attributes, newOp->start, newOp->end});
                freeAttributeTemps(ops, attributes);
                return {ops, resultPos};
            }
        }
        case 1: // integer
        {
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
        case 2: // float
        {
            char *end;
            double fltValue = strtod(Substr(ctx, node->nonTerm(0)).c_str(), &end);
            int64_t intValue = *(int64_t *)&fltValue;
            int64_t tmp = newTemp(ctx, getBaseType(ctx, "f64"));
            append(ops, {OP_NEW_FLOAT, {tmp, intValue}, {}, node->start, node->end});
            return {ops, tmp};
        }
        case 3: // identifer.identifer.identifer...
        {
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
        case 4: // fn call...
        {
            node = node->nonTerm(0);
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

            /* load function by name */
            WorkerDeclarationContext *fn = getWorkerByName(ctx, name);
            if (fn == NULL)
            {
                logError(ctx->filename, ctx->code, node->start, node->end, "can't find worker named %s", name.c_str());
                return {{}, -1};
            }
            printf("Call of %s...\n", name.c_str());

            args.push_back(ctx->result->workers[fn]);

            int64_t ind = 0;
            for (; ind < (int64_t)fn->inputs.size(); ++ind)
            {
                Node *ch = argList->nonTerm(ind);
                if (ch == NULL)
                {
                    logError(ctx->filename, ctx->code, argList->start, argList->end, "Too little parameters [%lld/%lld]\n", ind, fn->inputs.size());
                    return {{}, -1};
                }
                assert_type(ch, "expression");
                auto [res, pos] = buildExpression(ctx, ch);
                HANDLE_NOT_NULL(pos, ch);
                append(ops, res);
                args.push_back(pos);
                freeList.push_back(pos);
            }

            if (argList->nonTerm(ind))
            {
                logError(ctx->filename, ctx->code, argList->nonTerm(ind)->start, argList->nonTerm(ind)->end, "Too many parameters");
                return {{}, -1};
            }

            /* declare output variables */
            int64_t tmpResult = -1, outId = 0;
            vector<int64_t> outputs;
            for (auto ch : outList->childs)
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

            /* create all outputs */
            for (auto &i : outputs)
            {
                if (i != -1)
                {
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
                else
                {
                    if (ctx->variables[tmpResult]->type == TYPE_PIPE)
                    {
                        append(ops, {OP_NEW_PIPE, {tmpResult}, {}, outList->start, outList->end});
                    }
                    else if (ctx->variables[tmpResult]->type == TYPE_PROMISE)
                    {
                        append(ops, {OP_NEW_PROMISE, {tmpResult}, {}, outList->start, outList->end});
                    }
                    else
                    {
                        logError(ctx->filename, ctx->code, outList->start, outList->end, "Return types can be only pipe/promise, have %s", printType(ctx->variables[tmpResult]).c_str());
                    }
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
            // for (auto &i : freeList)
            // {
            //     freeTemp(ops, i);
            // }

            return {ops, tmpResult};
        }
    }
    return {{}, -1};
}

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
        case 1: // infix query
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
                break;
            }
            if (!is(node->nonTerm(2), "SimpleTerm") || node->nonTerm(2)->variant != 3)
            {
                logError(ctx->filename, ctx->code, node->nonTerm(2)->start, node->nonTerm(2)->end, "In infix form of class query, second part must be dotted identifer chain");
                break;
            }
            Node *path = node->nonTerm(2);
            /* get first field manually */
            Node *first = path->nonTerm(0);
            if (first == NULL)
            {
                logError(ctx->filename, ctx->code, path->start, path->end, "In infix form of class query, no path to field");
                break;
            }
            string fname = Substr(ctx, first);
            if (!type->_struct.names.contains(fname))
            {
                logError(ctx->filename, ctx->code, first->start, first->end, "In infix form of class query, no field named %s", fname.c_str());
                break;
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
    }
    return {{}, -1};
}


pair<vector<Operation>, int64_t> buildPrefixOperation(BuildContext *ctx, Node *node)
{
    if (!is(node, "PrefixOperation"))
    {
        return buildQueryOperation(ctx, node);
    }
    printf("Prefix operation\n");
    vector<Operation> ops;

    switch_var(node)
    {
        case 0: // operator
        {
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
        }
        case 2: // cast
        case 3: // cast
        {
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
    }
    return {{}, -1};
}

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
            switch_var(x)
            {
                case 0: logError(ctx->filename, ctx->code, x->start, x->end, "Can't push to NEW operator"); break;
                case 1: logError(ctx->filename, ctx->code, x->start, x->end, "Can't push to integer constant"); break;
                case 2: logError(ctx->filename, ctx->code, x->start, x->end, "Can't push to float constant"); break;
                case 3:
                {
                    /* push to variable/structure */
                    if (ctx->names.find(Substr(ctx, x->nonTerm(0))) == ctx->names.end())
                    {
                        logError(ctx->filename, ctx->code, x->nonTerm(0)->start, x->nonTerm(0)->end, "Unknown variable name: %s", Substr(ctx, x->nonTerm(0)).c_str());
                        break;
                    }
                    /* get variable */
                    int64_t varId = ctx->names[Substr(ctx, x->nonTerm(0))];
                    TypeContext *type = ctx->variables[varId];

                    auto [offset, fldType] = GetFieldOffset(ctx, x, 1, type);

                    /* check - if this is pushing of equal types */
                    if (is_castable(type, dataType))
                    {
                        if (is_convertable(type, dataType) || ctx->enabled("implicit_castes"))
                        {
                            if (type->type != TYPE_RECORD)
                            {
                                append(ops, {OP_MOV, {varId, dataPos}, {}, x->start, x->end});
                            }
                            else
                            {
                                append(ops, {OP_PUSH_VAR, {varId, offset, (int64_t)fldType, dataPos}, {}, x->start, x->end});
                            }
                        }
                        else
                        {
                            logError(ctx->filename, ctx->code, x->start, x->end, "can't automaticly cast this types, use <implicit_castes> flag to allow this cast: %s to %s", printType(dataType).c_str(), printType(type).c_str());
                        }
                    }
                    else
                    {
                        if (type->type == TYPE_PIPE)
                        {
                            if (is_castable(type->_vector.base, dataType))
                            {
                                if (is_convertable(type->_vector.base, dataType) || ctx->enabled("implicit_castes"))
                                {
                                    auto [target, targetPos] = buildSimpleTerm(ctx, x);
                                    HANDLE_NOT_NULL(targetPos, x);
                                    append(ops, target);
                                    append(ops, {OP_PUSH_PIPE, {targetPos, dataPos}, {}, x->start, x->end});
                                    freeTemp(ops, targetPos);
                                    break;
                                }
                                else
                                {
                                    logError(ctx->filename, ctx->code, x->start, x->end, "can't automaticly cast this types, use <implicit_castes> flag to allow this cast: %s to %s", printType(dataType).c_str(), printType(type).c_str());
                                    break;
                                }
                            }
                        }
                        else if (type->type == TYPE_PROMISE)
                        {
                            if (is_castable(type->_vector.base, dataType))
                            {
                                if (is_convertable(type->_vector.base, dataType) || ctx->enabled("implicit_castes"))
                                {
                                    auto [target, targetPos] = buildSimpleTerm(ctx, x);
                                    HANDLE_NOT_NULL(targetPos, x);
                                    append(ops, target);
                                    append(ops, {OP_PUSH_PROMISE, {targetPos, dataPos}, {}, x->start, x->end});
                                    freeTemp(ops, targetPos);
                                    break;
                                }
                                else
                                {
                                    logError(ctx->filename, ctx->code, x->start, x->end, "can't automaticly cast this types, use <implicit_castes> flag to allow this cast: %s to %s", printType(dataType).c_str(), printType(type).c_str());
                                    break;
                                }
                            }
                        }
                        logError(ctx->filename, ctx->code, x->start, x->end, "this types are uncastable - push is wrong: %s to %s", printType(dataType).c_str(), printType(type).c_str());
                    }
                    break;
                }
                case 4: logError(ctx->filename, ctx->code, x->start, x->end, "Can't push to worker call"); break;

            }
        }
        else if (is(x, "expression"))
        {
            /* get value - if it is pipe or it is promise - push to them */
            auto [code, pos] = buildExpression(ctx, x);
            HANDLE_NOT_NULL(pos, x);
            TypeContext *type = ctx->variables[pos];

            switch (type->type)
            {
                case TYPE_CLASS:
                case TYPE_UNION:
                case TYPE_RECORD:
                case TYPE_ARRAY:
                case TYPE_SCALAR:
                    logError(ctx->filename, ctx->code, x->start, x->end, "can't push to expression [from braces] with value of type %s", printType(type).c_str());
                    break;
                case TYPE_PIPE:
                {
                    append(ops, code);
                    append(ops, {OP_PUSH_PIPE, {pos, dataPos}, {}, x->start, x->end});
                    freeTemp(ops, pos);
                    break;
                }
                case TYPE_PROMISE:
                {
                    append(ops, code);
                    append(ops, {OP_PUSH_PROMISE, {pos, dataPos}, {}, x->start, x->end});
                    freeTemp(ops, pos);
                    break;
                }
            }
        }
        else if (is(x, "IndexOperation"))
        {
            auto [array, arrayPos] = buildSimpleTerm(ctx, x->nonTerm(0));
            HANDLE_NOT_NULL(arrayPos, x->nonTerm(0));
            auto [index, indexPos] = buildExpression(ctx, x->nonTerm(1));
            HANDLE_NOT_NULL(indexPos, x->nonTerm(1));

            TypeContext *arrayType = ctx->variables[arrayPos];
            TypeContext *indexType = ctx->variables[indexPos];
            if (arrayType->type != TYPE_ARRAY)
            {
                logError(ctx->filename, ctx->code, x->start, x->end, "push to indexation not in array, wrong type: %s", printType(arrayType).c_str());
                break;
            }
            if (indexType->type != TYPE_SCALAR || (SCALAR_TYPE(indexType->_scalar.kind) != SCALAR_I && SCALAR_TYPE(indexType->_scalar.kind) != SCALAR_U))
            {
                logError(ctx->filename, ctx->code, node->start, node->end, "Index variable isn't integer scalar: %s", printType(indexType).c_str());
                return {{}, -1};
            }

            TypeContext *type = arrayType->_vector.base;

            /* find path to field */
            auto [offset, fldType] = GetFieldOffset(ctx, x, 2, type);

            /* check - if this is pushing of equal types */
            if (is_castable(fldType, dataType))
            {
                if (is_convertable(fldType, dataType) || ctx->enabled("implicit_castes"))
                {
                    /* calculate arguments */
                    append(ops, array);
                    append(ops, index);

                    append(ops, {OP_PUSH_ARRAY, {arrayPos, indexPos, offset, (int64_t)fldType, dataPos}, {}, x->start, x->end});

                    freeTemp(ops, arrayPos);
                    freeTemp(ops, indexPos);
                }
                else
                {
                    logError(ctx->filename, ctx->code, x->start, x->end, "can't automaticly cast this types, use <implicit_castes> flag to allow this cast: %s to %s", printType(dataType).c_str(), printType(fldType).c_str());
                }
            }
            else
            {
                if (fldType->type == TYPE_PIPE)
                {
                    if (is_castable(fldType->_vector.base, dataType))
                    {
                        if (is_convertable(fldType->_vector.base, dataType) || ctx->enabled("implicit_castes"))
                        {
                            /* calculate array value */
                            auto [target, targetPos] = buildIndexOperation(ctx, x);
                            HANDLE_NOT_NULL(targetPos, x);
                            append(ops, target);
                            append(ops, {OP_PUSH_PIPE, {targetPos, dataPos}, {}, x->start, x->end});
                            freeTemp(ops, targetPos);
                            break;
                        }
                        else
                        {
                            logError(ctx->filename, ctx->code, x->start, x->end, "can't automaticly cast this types, use <implicit_castes> flag to allow this cast: %s to %s", printType(dataType).c_str(), printType(fldType).c_str());
                            break;
                        }
                    }
                }
                else if (fldType->type == TYPE_PROMISE)
                {
                    if (is_castable(fldType->_vector.base, dataType))
                    {
                        if (is_convertable(fldType->_vector.base, dataType) || ctx->enabled("implicit_castes"))
                        {
                            /* calculate array value */
                            auto [target, targetPos] = buildIndexOperation(ctx, x);
                            HANDLE_NOT_NULL(targetPos, x);
                            append(ops, target);
                            append(ops, {OP_PUSH_PROMISE, {targetPos, dataPos}, {}, x->start, x->end});
                            freeTemp(ops, targetPos);
                            break;
                        }
                        else
                        {
                            logError(ctx->filename, ctx->code, x->start, x->end, "can't automaticly cast this types, use <implicit_castes> flag to allow this cast: %s to %s", printType(dataType).c_str(), printType(fldType).c_str());
                            break;
                        }
                    }
                }
                logError(ctx->filename, ctx->code, x->start, x->end, "this types are uncastable - push is wrong: %s to %s", printType(dataType).c_str(), printType(fldType).c_str());
            }
            break;
        }
        else if (is(x, "QueryOperation"))
        {
            if (x->variant == 0)
            {
                /* get value - if it is pipe or it is promise - push to them */
                auto [code, pos] = buildQueryOperation(ctx, x);
                HANDLE_NOT_NULL(pos, x);
                if (pos == -1)
                {
                    logError(ctx->filename, ctx->code, x->start, x->end, "wrong usage of void expression [call without * is void]");
                    return {{}, -1};
                }
                TypeContext *type = ctx->variables[pos];

                switch (type->type)
                {
                    case TYPE_CLASS:
                    case TYPE_UNION:
                    case TYPE_RECORD:
                    case TYPE_ARRAY:
                    case TYPE_SCALAR:
                        logError(ctx->filename, ctx->code, x->start, x->end, "can't push to expression [from braces] with value of type %s", printType(type).c_str());
                        break;
                    case TYPE_PIPE:
                    {
                        append(ops, code);
                        append(ops, {OP_PUSH_PIPE, {pos, dataPos}, {}, x->start, x->end});
                        freeTemp(ops, pos);
                        break;
                    }
                    case TYPE_PROMISE:
                    {
                        append(ops, code);
                        append(ops, {OP_PUSH_PROMISE, {pos, dataPos}, {}, x->start, x->end});
                        freeTemp(ops, pos);
                        break;
                    }
                }
                break;
            }
            else // infix query syntax - class field access
            {
                auto [code, position] = buildIndexOperation(ctx, x->nonTerm(0));
                HANDLE_NOT_NULL(position, x->nonTerm(0));
                TypeContext *type = ctx->variables[position];
                auto [attributes, attrCode] = getAttributeList(ctx, x->nonTerm(1), true);
                append(ops, attrCode);
                if (type->type != TYPE_CLASS)
                {
                    logError(ctx->filename, ctx->code, x->nonTerm(0)->start, x->nonTerm(0)->end, "infix form of query must be used with classes, but used with %s", printType(type).c_str());
                    break;
                }

                Node *path = x->nonTerm(2);
                if (!is(path, "SimpleTerm") || path->variant != 3)
                {
                    logError(ctx->filename, ctx->code, path->start, path->end, "in infix form of class query, second part must be dotted identifer chain");
                    break;
                }

                /* get first field manually */
                Node *first = path->nonTerm(0);
                if (first == NULL)
                {
                    logError(ctx->filename, ctx->code, path->start, path->end, "In infix form of class query, no path to field");
                    break;
                }
                string fname = Substr(ctx, first);
                if (!type->_struct.names.contains(fname))
                {
                    logError(ctx->filename, ctx->code, first->start, first->end, "In infix form of class query, no field named %s", fname.c_str());
                    break;
                }
                TypeContext *pptype = type;
                type = type->_struct.fields[type->_struct.names[fname]];
                
                /* parse path to final field */
                auto [offset, fldType] = GetFieldOffset(ctx, path, 1, type);
                
                offset += GetFieldOffset(ctx, pptype, pptype->_struct.names[fname]).first;


                append(ops, code);
                append(ops, {OP_PUSH_CLASS, {position, offset, (int64_t)fldType, dataPos}, attributes, x->start, x->end});
                freeAttributeTemps(ops, attributes);
                freeTemp(ops, position);
                break;
            }
        }
    }
    return {ops, dataPos};
}

pair<vector<Operation>, int64_t> buildExpression(BuildContext *ctx, Node *node)
{
    assert_type(node, "expression");
    return buildSetOperation(ctx, node->nonTerm(0));
}

vector<Operation> buildStatement(BuildContext *ctx, Node *node)
{
    vector<Operation> ops;
    assert_type(node, "statement");
    switch_var(node)
    {
        case 0:
        {
            Node *child = node->nonTerm(0);
            assert_type(child, "var_declaration");
            if (is(child, "var_declaration"))
            {
                TypeContext *type = getType(ctx, child->nonTerm(0));
                if (type != NULL)
                {
                    for (auto name : child->childs)
                    {
                        if (is(name, "identifer"))
                        {
                            int64_t varId = ctx->nextVarId++;
                            ctx->variables[varId] = type;
                            ctx->names[Substr(ctx, name)] = varId;
                            printf("statement declaration [id=%lld, type=%p]\n", varId, type);
                        }
                    }
                }
            }
            return ops;
        }
        case 1:
        {
            printf("statement expression\n");
            auto [expr, exprPos] = buildExpression(ctx, node->nonTerm(0));
            // here -1 as expr pos is normal situation
            append(ops, expr);
            freeTemp(ops, exprPos);
            break;
        }
        case 2:
        {
            printf("statement while\n");
            auto [guard, guardPos] = buildExpression(ctx, node->nonTerm(0));
            if (handleNotNull(ctx, guardPos, node->nonTerm(0))) { break; }
            auto body = buildCodeBlock(ctx, node->nonTerm(1));
            append(ops, {OP_JMP, {2 + (int64_t)body.size()}, {}, node->start, node->end});
            freeTemp(ops, guardPos);
            append(ops, body);
            append(ops, guard);
            append(ops, {OP_JNZ, {- 1 - (int64_t)guard.size() - (int64_t)body.size(), guardPos}, {}, node->nonTerm(0)->start, node->nonTerm(0)->end});
            freeTemp(ops, guardPos);
            break;
        }
        case 3:
        {
            printf("statement match\n");
            auto [match, matchPos] = buildExpression(ctx, node->nonTerm(0));
            if (handleNotNull(ctx, matchPos, node->nonTerm(0))) { break; }
            ops.insert(ops.end(), match.begin(), match.end());
            for (auto &var : node->childs)
            {
                if (is(var, "case_branch"))
                {
                    switch_var(var)
                    {
                        case 0: // default
                        {
                            logError(ctx->filename, ctx->code, var->childs[1]->start, var->childs[1]->end, "default branches are unsupported for now");
                            break;
                        }
                        case 1: // expression
                        {
                            auto [pattern, patternPos] = buildExpression(ctx, var->nonTerm(0));
                            if (handleNotNull(ctx, patternPos, var->nonTerm(0))) { break; }
                            auto block = buildCodeBlock(ctx, var->nonTerm(1));
                            int64_t temp = newTemp(ctx, getBaseType(ctx, "i32"));
                            append(ops, pattern);
                            append(ops, {OP_EQ, {temp, matchPos, patternPos}, {}, var->start, var->end});
                            freeTemp(ops, patternPos);
                            append(ops, {OP_JZ, {2 + (int64_t)block.size(), temp}, {}, var->start, var->end});
                            freeTemp(ops, temp);
                            append(ops, block);
                            freeTemp(ops, temp);
                            break;
                        }
                    }
                }
            }
            freeTemp(ops, matchPos);
            break;
        }
        case 4:
        {
            printf("statement sleep\n");
            auto [var, varPos] = buildExpression(ctx, node->nonTerm(0));
            if (handleNotNull(ctx, varPos, node->nonTerm(0))) { break; }
            append(ops, var);
            append(ops, {OP_SLEEP, {varPos}, {}, node->start, node->end});
            freeTemp(ops, varPos);
            break;
        }
    }
    return ops;
}

vector<Operation> buildCodeBlock(BuildContext *ctx, Node *node)
{
    assert_type(node, "code_block");
    vector<Operation> ops;
    int64_t id = 0;
    while (node->nonTerm(id))
    {
        auto res = buildStatement(ctx, node->nonTerm(id));
        append(ops, res);
        id++;
    }
    return ops;
}

void buildWorkerContent(BuildContext *ctx, WorkerDeclarationContext *wk, Node *node, Node *wknode)
{
    assert_type(node, "code_block");
    printf("Building worker %s ...\n", wk->name.c_str());

    ctx->nextVarId = 0;
    ctx->nextTempId = FIRST_TEMP_ID;
    ctx->names.clear();
    ctx->variables.clear();
    ctx->current = wk;

    vector<Operation> ops;

    wk->content = new WorkerContext();
    /* create variables for inputs + code to load them */
    int64_t inputId = 0;
    for (auto &[name, type] : wk->inputs)
    {
        int64_t varId = ctx->names[name] = ctx->nextVarId++;
        ctx->variables[varId] = type;
        append(ops, {OP_LOAD_INPUT, {inputId++, varId}, {}, wknode->nonTerm(0)->start, wknode->nonTerm(0)->end});
    }
    /* create variables for outputs? */
    int64_t outputId = 0;
    for (auto &[name, type] : wk->outputs)
    {
        int64_t varId = ctx->names[name] = ctx->nextVarId++;
        ctx->variables[varId] = type;
        append(ops, {OP_LOAD_OUTPUT, {outputId++, varId}, {}, wknode->nonTerm(2)->start, wknode->nonTerm(2)->end});
    }
    /* build body */
    auto res = buildCodeBlock(ctx, node);
    append(ops, res);
    /* convert Operations to Blocks */
    vector<OperationBlock *> data(ops.size());
    map<OperationBlock *, int64_t> blockId;
    /* fill data */
    for (int64_t i = 0; i < (int64_t)ops.size(); ++i)
    {
        data[i] = new OperationBlock(ops[i].type, ops[i].data, ops[i].attributes, {}, {}, ops[i].code_start, ops[i].code_end);
        blockId[data[i]] = i;
    }
    /* fill links */
    for (int64_t i = 0; i < (int64_t)ops.size(); ++i)
    {
        if (data[i]->type == OP_JMP)
        {
            int64_t pos = data[i]->data.front();
            data[i]->data.erase(data[i]->data.begin());
            data[i]->next.push_back(i + pos >= (int64_t)data.size() ? NULL : data[i + pos]);
        }
        else
        {
            data[i]->next.push_back(i + 1 == (int64_t)ops.size() ? NULL : data[i + 1]);
            if (IS_JUMP(data[i]->type))
            {
                int64_t pos = data[i]->data.front();
                data[i]->data.erase(data[i]->data.begin());
                data[i]->next.push_back(i + pos >= (int64_t)data.size() ? NULL : data[i + pos]);
            }
        }
        for (auto &j : data[i]->next)
        {
            if (j != NULL)
            {
                data[blockId[j]]->prev.insert(data[i]);
            }
        }
    }
    /* set data */
    wk->content->entry = data.front();
    wk->content->code = data;
    wk->content->variables = ctx->variables;
    wk->content->nextVarId = ctx->nextVarId;
    wk->content->nextTempId = ctx->nextTempId;
    /* remove jumps */
    for (auto &[k, v] : blockId)
    {
        if (k->type == OP_JMP)
        {
            removeOp(wk, k);
        }
    }
}

void addWorkerDefinition(BuildContext *ctx, Node *node, bool with_code)
{
    if (with_code) { assert_type(node, "worker"); }
    else { assert_type(node, "worker_decl"); }


    WorkerDeclarationContext *wk = new WorkerDeclarationContext();

    wk->code_start = node->start;
    if (with_code)
    {
        wk->code_end = node->nonTerm(4)->start;
    }
    else
    {
        wk->code_end = node->end;
    }
    wk->code_block_end = node->end;

    tie(wk->attributes, ignore) = getAttributeList(ctx, node->nonTerm(0), false);

    /* read arguments */
    wk->inputs = readWorkerArgList(ctx, node->nonTerm(1));
    wk->outputs = readWorkerArgList(ctx, node->nonTerm(3));

    wk->name = Substr(ctx, node->nonTerm(2));

    if (wk->attributes.contains("entry"))
    {
        wk->used_providers.insert(ctx->entryProvider);
        ctx->result->used_providers.insert(ctx->entryProvider);
    }

    printf("Worker %s declaration generated\n", wk->name.data());

    ctx->result->workers[wk] = ctx->result->nextWorkerId++;
}

BuildContext *initializateContext(const char *filename, char *source, map<string, string> configs)
{
    BuildContext *ctx = new BuildContext(configs, filename, source);
    ctx->result = new BuildResult();
    ctx->result->filename = filename;
    ctx->result->code = source;
    ctx->result->cost = 0;
    ctx->result->nextWorkerId = 0;

    /* add builtin types */

    ctx->typeTable[{"i8", ""}] = new TypeContext(1, TYPE_SCALAR, "", {._scalar={SCALAR_I8}});
    ctx->typeTable[{"i16", ""}] = new TypeContext(2, TYPE_SCALAR, "", {._scalar={SCALAR_I16}});
    ctx->typeTable[{"i32", ""}] = new TypeContext(4, TYPE_SCALAR, "", {._scalar={SCALAR_I32}});
    ctx->typeTable[{"i64", ""}] = new TypeContext(8, TYPE_SCALAR, "", {._scalar={SCALAR_I64}});

    ctx->typeTable[{"u8", ""}] = new TypeContext(1, TYPE_SCALAR, "", {._scalar={SCALAR_U8}});
    ctx->typeTable[{"u16", ""}] = new TypeContext(2, TYPE_SCALAR, "", {._scalar={SCALAR_U16}});
    ctx->typeTable[{"u32", ""}] = new TypeContext(4, TYPE_SCALAR, "", {._scalar={SCALAR_U32}});
    ctx->typeTable[{"u64", ""}] = new TypeContext(8, TYPE_SCALAR, "", {._scalar={SCALAR_U64}});

    ctx->typeTable[{"f32", ""}] = new TypeContext(4, TYPE_SCALAR, "", {._scalar={SCALAR_F32}});
    ctx->typeTable[{"f64", ""}] = new TypeContext(8, TYPE_SCALAR, "", {._scalar={SCALAR_F64}});
    
    ctx->typeTable[{"object", ""}] = new TypeContext(8, TYPE_CLASS, "", {._struct={}});

    for (auto &[k, v] : ctx->typeTable)
    {
        ctx->types.push_back(v);
    }

    return ctx;
}


Node *compressNode(Node *x)
{
    if (!x->rule) { return x; }
    if (x->rule->variants[x->variant].pass)
    {
        return compressNode(x->nonTerm(0));
    }
    for (auto &t : x->childs)
    {
        t = compressNode(t);
    }
    return x;
}


pair<BuildResult *, bool> buildAst(const char *filename, char *source, vector<Node *>nodes, map<string, string> configs, string entryProvider)
{
    /* compress AST tree */
    for (auto &node : nodes)
    {
        node = compressNode(node);
        dumpAst(node);
    }
    BuildContext *ctx = initializateContext(filename, source, configs);
    ctx->entryProvider = entryProvider;
    for (auto &node : nodes)
    {
        assert_type(node, "Global");
        assert(node->childs.size() == 1);

        if (is(node->childs[0], "S"))
        {
            /* skip empty node */
        }
        else if (is(node->childs[0], "_using"))
        {
            string provider = Substr(ctx, node->childs[0]->nonTerm(0));
            if (validateProviderWithError(ctx, node, provider))
            {
                ctx->provider = provider;
            }
        }
        else if (is(node->childs[0], "_record"))
        {
            processStructure(ctx, TYPE_RECORD, node->childs[0]);
        }
        else if (is(node->childs[0], "_union"))
        {
            processStructure(ctx, TYPE_UNION, node->childs[0]);
        }
        else if (is(node->childs[0], "_class"))
        {
            processStructure(ctx, TYPE_CLASS, node->childs[0]);
        }
        else if (is(node->childs[0], "worker"))
        {
            addWorkerDefinition(ctx, node->childs[0], true);
        }
        else if (is(node->childs[0], "worker_decl"))
        {
            addWorkerDefinition(ctx, node->childs[0], false);
        }
        else
        {
            logError(filename, source, node->childs[0]->start, "Unknown global node type: <%s>\n", node->childs[0]->rule->name);
        }
    }
    ctx->provider = "";
    for (auto &node : nodes)
    {
        assert_type(node, "Global");
        assert(node->childs.size() == 1);

        if (is(node->childs[0], "_using"))
        {
            string provider = Substr(ctx, node->childs[0]->nonTerm(0));
            if (validateProviderWithError(ctx, node, provider))
            {
                ctx->provider = provider;
            }
        }
        if (is(node->childs[0], "worker"))
        {
            Node *code = node->childs[0]->nonTerm(4);
            WorkerDeclarationContext *wk = getWorkerByName(ctx, Substr(ctx, node->childs[0]->nonTerm(2)));
            buildWorkerContent(ctx, wk, code, node->childs[0]);
        }
    }

    int64_t id = 0;
    for (auto &[fn, key] : ctx->result->workers)
    {
        ctx->result->names[fn->name] = id++;
    }

    // delete ctx;
    return {ctx->result, false};
}

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
