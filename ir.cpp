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
    map<string, string> attributes = {};
};


pair<vector<Operation>, int64_t> buildExpression(BuildContext *ctx, Node *node);
vector<Operation> buildCodeBlock(BuildContext *ctx, Node *node);

void append(vector<Operation> &a, vector<Operation> &b)
{
    a.insert(a.end(), b.begin(), b.end());
}

int64_t append(vector<Operation> &a, Operation b)
{
    a.push_back(b);
    return a.size() - 1;
}

string Substr(BuildContext *ctx, Node *node)
{
    return string(ctx->code + node->start, node->end - node->start);
}

void printTypeR(TypeContext *t)
{
    switch (t->type)
    {
        case TYPE_UNION: printf("UNION"); break;
        case TYPE_RECORD: printf("RECORD"); break;
        case TYPE_CLASS: printf("CLASS"); break;
        case TYPE_PIPE: printf("PIPE of "); printTypeR(t->_vector.base); break;
        case TYPE_ARRAY: printf("ARRAY of "); printTypeR(t->_vector.base); break;
        case TYPE_PROMISE: printf("PROMISE of "); printTypeR(t->_vector.base); break;
        case TYPE_SCALAR: printf("SCALAR [%d, size=%lld]", t->_scalar.kind, t->size); break;
    }
}
void printType(TypeContext *t)
{
    printTypeR(t); printf("\n");
}

#define HANDLE_NOT_NULL(tmp, node) \
    if (handleNotNull(ctx, tmp, node)) return {{}, -1};
    
bool handleNotNull(BuildContext *ctx, int64_t tmp, Node *node)
{    
    if (tmp == -1)
    {
        printf("Error: wrong usage of void expression\n");
        logError(ctx->filename, ctx->code, node->start, node->end);
        return true;
    }
    return false;
}

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

TypeContext *getDerivative(BuildContext *ctx, TypeContext *type, TypeContextType derivative)
{
    for (auto &val : ctx->types)
    {
        if (val->type == derivative && val->_vector.base == type)
        {
            return val;
        }
    }
    ctx->types.push_back(new TypeContext(8, derivative, {._vector={type}}));
    return ctx->types.back();
}

TypeContext *getBaseType(BuildContext *ctx, string name)
{
    return ctx->typeTable[name];
}

TypeContext *getIntegerType(BuildContext *ctx, int64_t x)
{
    // TODO: how can i do this?
    // if (x >= INT8_MIN && x <= INT8_MAX) return getBaseType(ctx, "i8");
    // if (x >= 0 && (uint64_t)x <= UINT8_MAX) return getBaseType(ctx, "u8");
    // if (x >= INT16_MIN && x <= INT16_MAX) return getBaseType(ctx, "i16");
    // if (x >= 0 && (uint64_t)x <= UINT16_MAX) return getBaseType(ctx, "u16");
    if (x >= INT32_MIN && x <= INT32_MAX)  return getBaseType(ctx, "i32");
    if (x >= 0 && (uint64_t)x <= UINT32_MAX) return getBaseType(ctx, "u32");
    return getBaseType(ctx, "i64");
}

TypeContext *getType(BuildContext *ctx, Node *node)
{
    assert_type(node, "var_type");
    /* find base type */
    string baseName = Substr(ctx, node->nonTerm(0));
    TypeContext *cur = ctx->typeTable[baseName];
    for (auto &child : node->childs)
    {
        if (is(child, "var_type_moditifer"))
        {
            cur = getDerivative(ctx, cur, vector<TypeContextType>{TYPE_ARRAY, TYPE_PIPE, TYPE_PROMISE}[child->variant]);
        }
    }
    return cur;
}

map<string, string> getAttributeList(BuildContext *ctx, Node *node)
{
    map<string, string> attrs;
    int64_t attrId = 0;
    assert_type(node, "attribute_list");
    while (node->nonTerm(attrId))
    {
        Node *key = node->nonTerm(attrId + 0);
        Node *value = node->nonTerm(attrId + 1);
        assert_type(key, "identifer_or_number");
        assert_type(value, "identifer_or_number");
        attrs[Substr(ctx, key)] = Substr(ctx, value);
        attrId += 2;
    }
    return attrs;
}

pair<int64_t, TypeContext *> GetFieldOffset(BuildContext *ctx, TypeContext *type, int64_t field)
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

pair<int64_t, TypeContext *> GetFieldOffset(BuildContext *ctx, Node *node, int64_t fromId, TypeContext *type)
{    
    int64_t id = fromId;
    int64_t offset = 0;
    while (node->nonTerm(id))
    {                
        string field = Substr(ctx, node->nonTerm(id));
        if (type->type != TYPE_RECORD && type->type != TYPE_UNION)
        {
            printf("[Usage of dot on not structure/union object]\n");
            logError(ctx->filename, ctx->code, node->nonTerm(id)->start, node->nonTerm(id)->end);
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
            printf("Error: processStructure called with not TYPE_RECORD/TYPE_UNION/TYPE_CLASS\n");
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
    printf("Type %s generated\n", name.data());
    for (auto &[k, v] : names)
    {
        printf("Field %s -> type %p\n", k.data(), fields[v]);
    }
    ctx->types.push_back(new TypeContext(total_size, type, {._struct={fields, names}}));
    ctx->typeTable[name] = ctx->types.back();
}

vector<pair<string, TypeContext *>> readWorkerArgList(BuildContext *ctx, Node *node)
{
    assert_type(node, "arguments_list");
    vector<pair<string, TypeContext *>> res;

    int64_t id = 0;
    while (node->nonTerm(id))
    {
        TypeContext *type = getType(ctx, node->nonTerm(id + 0));
        string name = Substr(ctx, node->nonTerm(id + 1));
        printf("Input %s of type %p\n", name.data(), type);
        res.push_back({name, type});
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
        code.push_back({OP_FREE_TEMP, {id}});
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

            TypeContext *type = getType(ctx, newOp->nonTerm(0));
            map<string, string> attributes = getAttributeList(ctx, newOp->nonTerm(1));
            
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
                    printf("Error: can't use NEW expression with union/record/scalar types\n");
                    logError(ctx->filename, ctx->code, newOp->start, newOp->end);
                    return {{}, -1};
                case TYPE_CLASS: 
                {
                    if (args.size() > 0)
                    {
                        printf("Error: NEW expression with class doesn't support field initialization for now\n");
                        logError(ctx->filename, ctx->code, newOp->start, newOp->end);
                        return {{}, -1};
                    }
                    int64_t resultPos = newTemp(ctx, type);
                    append(ops, {OP_NEW_CLASS, {resultPos}, attributes});
                    return {ops, resultPos};
                }
                case TYPE_ARRAY:
                {
                    if (args.size() != 1)
                    {
                        printf("Error: NEW expression with array need 1 parameter - length\n");
                        logError(ctx->filename, ctx->code, newOp->start, newOp->end);
                        return {{}, -1};
                    }
                    int64_t resultPos = newTemp(ctx, type);
                    append(ops, {OP_NEW_ARRAY, {resultPos, args[0]}, attributes});
                    return {ops, resultPos};
                }
                case TYPE_PIPE: 
                {
                    if (args.size() > 0)
                    {
                        printf("Error: NEW expression with pipe can't take any parameters\n");
                        logError(ctx->filename, ctx->code, newOp->start, newOp->end);
                        return {{}, -1};
                    }
                    int64_t resultPos = newTemp(ctx, type);
                    append(ops, {OP_NEW_PIPE, {resultPos}, attributes});
                    return {ops, resultPos};
                }
                case TYPE_PROMISE: 
                {
                    if (args.size() > 0)
                    {
                        printf("Error: NEW expression with promise can't take any parameters\n");
                        logError(ctx->filename, ctx->code, newOp->start, newOp->end);
                        return {{}, -1};
                    }
                    int64_t resultPos = newTemp(ctx, type);
                    append(ops, {OP_NEW_PROMISE, {resultPos}, attributes});
                    return {ops, resultPos};
                } 
            }
            return {{}, -1};
        }
        case 1: // integer
        {
            char *end;
            int64_t intValue = strtoll(Substr(ctx, node->nonTerm(0)).c_str(), &end, 0);
            int64_t tmp = newTemp(ctx, getIntegerType(ctx, intValue));
            append(ops, {OP_NEW_INT, {tmp, intValue}});
            return {ops, tmp};
        }
        case 2: // float
        {
            char *end;
            double value = strtod(Substr(ctx, node->nonTerm(0)).c_str(), &end);
            int64_t intValue = *(int64_t *)&value;
            int64_t tmp = newTemp(ctx, getBaseType(ctx, "f64"));
            append(ops, {OP_NEW_FLOAT, {tmp, intValue}});
            return {ops, tmp};
        }
        case 3: // identifer.identifer.identifer...
        {
            /* load variable */
            string name = Substr(ctx, node->nonTerm(0));
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
                append(ops, {OP_MOV, {tmp, varId}});
            }
            else
            {
                append(ops, {OP_QUERY_VAR, {tmp, varId, offset, (int64_t)fldType}});
            }
            return {ops, tmp};
        }
        case 4: // fn call...
        {
            Node *argList = node->nonTerm(0);
            string name = Substr(ctx, node->nonTerm(1));
            map<string, string> attributes = getAttributeList(ctx, node->nonTerm(2));
            Node *outList = node->nonTerm(3);

            /* build all arguments */
            vector<int64_t> args;
            
            /* load function by name */
            WorkerDeclarationContext *fn = getWorkerByName(ctx, name);
            if (fn == NULL)
            {
                printf("Error: can't find worker named %s\n", name.c_str());
                logError(ctx->filename, ctx->code, node->start, node->end);
                return {{}, -1};
            }
            printf("Call of %s...\n", name.c_str());

            args.push_back(ctx->result->workers[fn]);
            
            for (auto ch : argList->childs)
            {
                if (is(ch, "expression"))
                {
                    auto [res, pos] = buildExpression(ctx, ch);
                    HANDLE_NOT_NULL(pos, ch);
                    append(ops, res);
                    args.push_back(pos);
                }
            }
            
            /* declare output variables */
            int64_t tmpResult = -1, outId = 0;
            vector<int64_t> outputs;
            for (auto ch : outList->childs)
            {
                if (is(ch, "result_list_identifer"))
                {
                    switch_var(ch)
                    {
                        case 0: /* "star" output - this is return type */
                        {
                            if (tmpResult != -1)
                            {
                                printf("Two or more * in outputs of call - expression can have only one value\n");
                                logError(ctx->filename, ctx->code, ch->start, ch->end);
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
                        }
                    }
                }
                outId++;
            }

            /* create all outputs */
            for (auto &i : outputs)
            {
                if (i != -1)
                {
                    if (ctx->variables[i]->type == TYPE_PIPE)
                    {
                        append(ops, {OP_NEW_PIPE, {i}});
                    }
                    else if (ctx->variables[i]->type == TYPE_PROMISE)
                    {
                        append(ops, {OP_NEW_PROMISE, {i}});
                    }
                    else
                    {
                        printf("Return types can be only pipe/promise, have\n");
                        printType(ctx->variables[i]);
                        logError(ctx->filename, ctx->code, outList->start, outList->end);
                    }
                }
                else
                {
                    if (ctx->variables[tmpResult]->type == TYPE_PIPE)
                    {
                        append(ops, {OP_NEW_PIPE, {tmpResult}});
                    }
                    else if (ctx->variables[tmpResult]->type == TYPE_PROMISE)
                    {
                        append(ops, {OP_NEW_PROMISE, {tmpResult}});
                    }
                    else
                    {
                        printf("Return types can be only pipe/promise, have\n");
                        printType(ctx->variables[tmpResult]);
                        logError(ctx->filename, ctx->code, outList->start, outList->end);
                    }
                }
            }

            /* create operation */
            append(ops, {OP_CALL, args, attributes});
            
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
            printf("Can't use index on class/union/record/scalar/pipe/promise\n");
            printType(type);
            logError(ctx->filename, ctx->code, node->start, node->end);
            return {{}, -1};
        case TYPE_ARRAY:
        {
            TypeContext *indexType = ctx->variables[posIndex];
            if (indexType->type != TYPE_SCALAR || (SCALAR_TYPE(indexType->_scalar.kind) != SCALAR_I && SCALAR_TYPE(indexType->_scalar.kind) != SCALAR_U))
            {
                printf("Index variable isn't integer scalar\n");
                printType(type);
                logError(ctx->filename, ctx->code, node->start, node->end);
                return {{}, -1};
            }
            append(ops, t);
            append(ops, tIndex);
            /* generate path */
            auto [offset, fldType] = GetFieldOffset(ctx, node, 2, type->_vector.base);
            int64_t tmp = newTemp(ctx, fldType);
            append(ops, {OP_QUERY_INDEX, {tmp, pos, offset, (int64_t)fldType, posIndex}});
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
                    printf("Can't use prefix query on class/union/record/scalar\n");
                    printType(elemType);
                    logError(ctx->filename, ctx->code, node->start, node->end);
                    return {{}, -1};
                case TYPE_ARRAY:
                {
                    int64_t tmp = newTemp(ctx, getBaseType(ctx, "i64"));
                    append(ops, {OP_QUERY_ARRAY, {tmp, pos}}); 
                    freeTemp(ops, pos);
                    return {ops, tmp};
                }
                case TYPE_PIPE:
                {
                    int64_t tmp = newTemp(ctx, elemType->_vector.base);
                    append(ops, {OP_QUERY_PIPE, {tmp, pos}}); 
                    freeTemp(ops, pos);
                    return {ops, tmp};
                }
                case TYPE_PROMISE:
                {
                    int64_t tmp = newTemp(ctx, elemType->_vector.base);
                    append(ops, {OP_QUERY_PROMISE, {tmp, pos}}); 
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
            map<string, string> attributes = getAttributeList(ctx, node->nonTerm(1));
            if (type->type != TYPE_CLASS)
            {
                printf("[infix form of query must be used with classes, but used with:]\n");
                printType(type);
                logError(ctx->filename, ctx->code, node->nonTerm(0)->start, node->nonTerm(0)->end);       
                break;
            }
            if (!is(node->nonTerm(2), "SimpleTerm") || node->nonTerm(2)->variant != 3)
            {
                printf("[Error - in infix form of class query, second part must be dotted identifer chain]\n");
                logError(ctx->filename, ctx->code, node->nonTerm(2)->start, node->nonTerm(2)->end);
                break;
            }
            Node *path = node->nonTerm(2);
            /* parse path to field */
            auto [offset, fldType] = GetFieldOffset(ctx, path, 0, type);
            int64_t tmp = newTemp(ctx, fldType);
            append(ops, code);
            append(ops, {OP_QUERY_CLASS, {tmp, position, offset, (int64_t)fldType}, attributes});
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
                    append(ops, {OP_NEW_INT, {tmp, 0}}); 
                    append(ops, {OP_SUB, {pos, tmp, pos}}); 
                    freeTemp(ops, tmp);
                    break; 
                }
                case 2: // !
                { 
                    int64_t tmp = newTemp(ctx, getBaseType(ctx, "i32"));
                    append(ops, {OP_JZ, {3, pos}});
                    append(ops, {OP_NEW_INT, {tmp, -1}});
                    append(ops, {OP_JMP, {2, pos}});
                    append(ops, {OP_NEW_INT, {tmp, 0}});
                    freeTemp(ops, pos);
                    resultPosition = tmp;
                    break; 
                }
                case 3: // ~ 
                {  append(ops, {OP_BNOT, {pos, pos}}); break; }
            }
            return {ops, resultPosition};
        } 
        case 1: // cast
        {
            auto [t, pos] = buildPrefixOperation(ctx, node->nonTerm(1));
            HANDLE_NOT_NULL(pos, node->nonTerm(1));

            TypeContext *type = getType(ctx, node->nonTerm(0));

            if (!is_castable(type, ctx->variables[pos]))
            {
                printf("Cast is impossible between this types\n");
                printType(type);
                printType(ctx->variables[pos]);
                logError(ctx->filename, ctx->code, node->start, node->end);
            }
            
            append(ops, t);
            int64_t tmp = newTemp(ctx, type);
            append(ops, {OP_CAST, {pos, tmp, pos}}); 
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
            printf("Can't implictly add this types\n");
            printType(ctx->variables[pos]);
            printType(ctx->variables[pos2]);
            logError(ctx->filename, ctx->code, node->nonTerm(id + 0)->start, node->nonTerm(id + 0)->end);
            return {{}, -1};
        }
        
        int64_t tmp = newTemp(ctx, resType);
        
        append(ops, t);
        append(ops, t2);
        
        switch_var(node->nonTerm(id + 0))
        {
            case 0: // |
            { append(ops, {OP_BOR, {tmp, pos, pos2}}); break; }
            case 1: // &
            { append(ops, {OP_BAND, {tmp, pos, pos2}}); break; }
            case 2: // ^
            { append(ops, {OP_BXOR, {tmp, pos, pos2}}); break; }
            case 3: // <<
            { append(ops, {OP_SHL, {tmp, pos, pos2}}); break; }
            case 4: // >>
            { append(ops, {OP_SHR, {tmp, pos, pos2}}); break; }
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
            printf("Can't implictly add this types\n");
            printType(ctx->variables[pos]);
            printType(ctx->variables[pos2]);
            logError(ctx->filename, ctx->code, node->nonTerm(id + 0)->start, node->nonTerm(id + 0)->end);
            return {{}, -1};
        }
        
        int64_t tmp = newTemp(ctx, resType);
        
        append(ops, t);
        append(ops, t2);
        
        switch_var(node->nonTerm(id + 0))
        {
            case 0: // *
            { append(ops, {OP_MUL, {tmp, pos, pos2}}); break; }
            case 1: // /
            { append(ops, {OP_DIV, {tmp, pos, pos2}}); break; }
            case 2: // %
            { append(ops, {OP_MOD, {tmp, pos, pos2}}); break; }
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
            printf("Can't implictly add this types\n");
            printType(ctx->variables[pos]);
            printType(ctx->variables[pos2]);
            logError(ctx->filename, ctx->code, node->nonTerm(id + 0)->start, node->nonTerm(id + 0)->end);
            return {{}, -1};
        }
        
        int64_t tmp = newTemp(ctx, resType);

        append(ops, t);
        append(ops, t2);
        
        switch_var(node->nonTerm(id + 0))
        {
            case 0: // +
            { append(ops, {OP_ADD, {tmp, pos, pos2}}); break; }
            case 1: // -
            { append(ops, {OP_SUB, {tmp, pos, pos2}}); break; }
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
        
        int64_t tmp = newTemp(ctx, getBaseType(ctx, "i32"));
        
        append(ops, t);
        append(ops, t2);
        
        switch_var(node->nonTerm(id + 0))
        {
            case 0: // <> 
            { append(ops, {OP_NE, {tmp, pos, pos2}}); break; }
            case 1: // >=
            { append(ops, {OP_GE, {tmp, pos, pos2}}); break; }
            case 2: // <=
            { append(ops, {OP_LE, {tmp, pos, pos2}}); break; }
            case 3: // >
            { append(ops, {OP_GT, {tmp, pos, pos2}}); break; }
            case 4: // <
            { append(ops, {OP_LT, {tmp, pos, pos2}}); break; }
            case 5: // =
            { append(ops, {OP_EQ, {tmp, pos, pos2}}); break; }
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
        int64_t tmp = newTemp(ctx, getBaseType(ctx, "i32"));
        switch_var(node->nonTerm(id + 0))
        {
            case 0: // &&
            {
                append(ops, t);
                int64_t A_jmp = append(ops, {OP_JZ, {-1, pos}});
                freeTemp(ops, pos);
                append(ops, t2);
                append(ops, {OP_NEW_INT, {tmp, -1}});
                int64_t B_jmp = append(ops, {OP_JNZ, {-1, pos2}});
                freeTemp(ops, pos2);
                int64_t pushFalse = append(ops, {OP_NEW_INT, {tmp, 0}});
                
                ops[A_jmp].data[0] = pushFalse - A_jmp;
                ops[B_jmp].data[0] = ops.size() - B_jmp;
                break;
            }
            case 1: // ||
            {
                append(ops, t);
                int64_t A_jmp = append(ops, {OP_JNZ, {-1, pos}});
                freeTemp(ops, pos);
                append(ops, t2);
                append(ops, {OP_NEW_INT, {tmp, 0}});
                int64_t B_jmp = append(ops, {OP_JZ, {-1, pos2}});
                freeTemp(ops, pos2);
                int64_t pushTrue = append(ops, {OP_NEW_INT, {tmp, -1}});
                
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
                case 0: printf("[Can't push to NEW operator]\n"); logError(ctx->filename, ctx->code, x->start, x->end); break;
                case 1: printf("[Can't push to integer constant]\n"); logError(ctx->filename, ctx->code, x->start, x->end); break;
                case 2: printf("[Can't push to float constant]\n"); logError(ctx->filename, ctx->code, x->start, x->end); break;
                case 3:
                {
                    /* push to variable/structure */
                    if (ctx->names.find(Substr(ctx, x->nonTerm(0))) == ctx->names.end())
                    {
                        printf("[Unknown variable name: %s]\n", Substr(ctx, x->nonTerm(0)).c_str());
                        logError(ctx->filename, ctx->code, x->nonTerm(0)->start, x->nonTerm(0)->end);
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
                                append(ops, {OP_MOV, {varId, dataPos}});
                            }
                            else
                            {
                                append(ops, {OP_PUSH_VAR, {varId, offset, (int64_t)fldType, dataPos}});
                            }
                        }
                        else
                        {
                            printf("[can't automaticly cast this types, use <implicit_castes> flag to allow this cast]\n");
                            printType(type);
                            printType(dataType);
                            logError(ctx->filename, ctx->code, x->start, x->end);   
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
                                    append(ops, {OP_PUSH_PIPE, {targetPos, dataPos}});
                                    freeTemp(ops, targetPos);
                                    break;
                                }
                                else
                                {
                                    printf("[can't automaticly cast this types, use <implicit_castes> flag to allow this cast]\n");
                                    printType(type);
                                    printType(dataType);
                                    logError(ctx->filename, ctx->code, x->start, x->end);   
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
                                    append(ops, {OP_PUSH_PROMISE, {targetPos, dataPos}});
                                    freeTemp(ops, targetPos);
                                    break;
                                }
                                else
                                {
                                    printf("[can't automaticly cast this types, use <implicit_castes> flag to allow this cast]\n");
                                    printType(type);
                                    printType(dataType);
                                    logError(ctx->filename, ctx->code, x->start, x->end);   
                                    break;
                                } 
                            }
                        }
                        printf("[this types are uncastable - push is wrong]\n");
                        printType(type);
                        printType(dataType);
                        logError(ctx->filename, ctx->code, x->start, x->end);       
                    }
                    break;
                }
                case 4: printf("[Can't push to worker call]\n"); logError(ctx->filename, ctx->code, x->start, x->end); break;

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
                    printf("[can't push to expression [from braces] with value of type class/union/record/array/scalar:]\n");
                    printType(type);
                    logError(ctx->filename, ctx->code, x->start, x->end);       
                    break;
                case TYPE_PIPE:
                {
                    append(ops, code);
                    append(ops, {OP_PUSH_PIPE, {pos, dataPos}});
                    freeTemp(ops, pos);
                    break;
                }
                case TYPE_PROMISE:
                {
                    append(ops, code);
                    append(ops, {OP_PUSH_PROMISE, {pos, dataPos}});
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
                printf("[push to indexation not in array, wrong type:]\n");
                printType(arrayType);
                logError(ctx->filename, ctx->code, x->start, x->end);   
                break;
            }            
            if (indexType->type != TYPE_SCALAR || (SCALAR_TYPE(indexType->_scalar.kind) != SCALAR_I && SCALAR_TYPE(indexType->_scalar.kind) != SCALAR_U))
            {
                printf("Index variable isn't integer scalar\n");
                printType(indexType);
                logError(ctx->filename, ctx->code, node->start, node->end);
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
                    
                    append(ops, {OP_PUSH_ARRAY, {arrayPos, indexPos, offset, (int64_t)fldType, dataPos}});

                    freeTemp(ops, arrayPos);
                    freeTemp(ops, indexPos);
                }
                else
                {
                    printf("[can't automaticly cast this types, use <implicit_castes> flag to allow this cast]\n");
                    printType(fldType);
                    printType(dataType);
                    logError(ctx->filename, ctx->code, x->start, x->end);   
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
                            append(ops, {OP_PUSH_PIPE, {targetPos, dataPos}});
                            freeTemp(ops, targetPos);
                            break;
                        }
                        else
                        {
                            printf("[can't automaticly cast this types, use <implicit_castes> flag to allow this cast]\n");
                            printType(fldType);
                            printType(dataType);
                            logError(ctx->filename, ctx->code, x->start, x->end);   
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
                            append(ops, {OP_PUSH_PROMISE, {targetPos, dataPos}});
                            freeTemp(ops, targetPos);
                            break;
                        }
                        else
                        {
                            printf("[can't automaticly cast this types, use <implicit_castes> flag to allow this cast]\n");
                            printType(fldType);
                            printType(dataType);
                            logError(ctx->filename, ctx->code, x->start, x->end);   
                            break;
                        } 
                    }
                }
                printf("[this types are uncastable - push is wrong]\n");
                printType(fldType);
                printType(dataType);
                logError(ctx->filename, ctx->code, x->start, x->end);       
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
                    printf("Error: wrong usage of void expression [call without * is void]\n");
                    logError(ctx->filename, ctx->code, x->start, x->end);
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
                        printf("[can't push to expression [from braces] with value of type class/union/record/array/scalar:]\n");
                        printType(type);
                        logError(ctx->filename, ctx->code, x->start, x->end);       
                        break;
                    case TYPE_PIPE:
                    {
                        append(ops, code);
                        append(ops, {OP_PUSH_PIPE, {pos, dataPos}});
                        freeTemp(ops, pos);
                        break;
                    }
                    case TYPE_PROMISE:
                    {
                        append(ops, code);
                        append(ops, {OP_PUSH_PROMISE, {pos, dataPos}});
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
                map<string, string> attributes = getAttributeList(ctx, x->nonTerm(1));
                if (type->type != TYPE_CLASS)
                {
                    printf("[infix form of query must be used with classes, but used with:]\n");
                    printType(type);
                    logError(ctx->filename, ctx->code, x->nonTerm(0)->start, x->nonTerm(0)->end);       
                    break;
                }
                
                Node *path = x->nonTerm(2);
                if (!is(path, "SimpleTerm") || path->variant != 3)
                {
                    printf("[Error - in infix form of class query, second part must be dotted identifer chain]\n");
                    logError(ctx->filename, ctx->code, path->start, path->end);
                    break;
                }
                
                /* parse path to field */
                auto [offset, fldType] = GetFieldOffset(ctx, path, 0, type);

                append(ops, code);
                append(ops, {OP_PUSH_CLASS, {position, offset, (int64_t)fldType, dataPos}, attributes});
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
            append(ops, {OP_JMP, {2 + (int64_t)body.size()}});
            freeTemp(ops, guardPos);
            append(ops, body);
            append(ops, guard);
            append(ops, {OP_JNZ, {- 1 - (int64_t)guard.size() - (int64_t)body.size(), guardPos}});
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
                            printf("default branches are unsupported for now\n");
                            logError(ctx->filename, ctx->code, var->childs[1]->start, var->childs[1]->end);
                            break;
                        }
                        case 1: // expression
                        {
                            auto [pattern, patternPos] = buildExpression(ctx, var->nonTerm(0));
                            if (handleNotNull(ctx, patternPos, var->nonTerm(0))) { break; }
                            auto block = buildCodeBlock(ctx, var->nonTerm(1));
                            int64_t temp = ctx->nextTempId++;
                            append(ops, pattern);
                            append(ops, {OP_EQ, {temp, matchPos, patternPos}});
                            freeTemp(ops, patternPos);
                            append(ops, {OP_JNZ, {2 + (int64_t)block.size(), temp}});
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

void buildWorkerContent(BuildContext *ctx, WorkerDeclarationContext *wk, Node *node)
{
    assert_type(node, "code_block");
    printf("Building worker %s ...\n", wk->name.c_str());
    
    ctx->nextVarId = 0;
    ctx->nextTempId = FIRST_TEMP_ID;
    ctx->names.clear();
    ctx->variables.clear();

    vector<Operation> ops;
    
    wk->content = new WorkerContext();
    /* create variables for inputs + code to load them */
    int64_t inputId = 0;
    for (auto &[name, type] : wk->inputs)
    {
        int64_t varId = ctx->names[name] = ctx->nextVarId++;
        ctx->variables[varId] = type;
        append(ops, {OP_LOAD_INPUT, {inputId++, varId}});
    }
    /* create variables for outputs? */
    int64_t outputId = 0;
    for (auto &[name, type] : wk->outputs)
    {
        int64_t varId = ctx->names[name] = ctx->nextVarId++;
        ctx->variables[varId] = type;
        append(ops, {OP_LOAD_OUTPUT, {outputId++, varId}});
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
        data[i] = new OperationBlock(ops[i].type, ops[i].data, ops[i].attributes, {});
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

    wk->attributes = getAttributeList(ctx, node->nonTerm(0));

    /* read arguments */
    wk->inputs = readWorkerArgList(ctx, node->nonTerm(1));
    wk->outputs = readWorkerArgList(ctx, node->nonTerm(3));
    
    wk->name = Substr(ctx, node->nonTerm(2));
    
    printf("Worker %s declaration generated\n", wk->name.data());

    ctx->result->workers[wk] = ctx->result->nextWorkerId++;
}

BuildContext *initializateContext(const char *filename, char *source, map<string, string> configs)
{
    BuildContext *ctx = new BuildContext(configs, filename, source);
    ctx->result = new BuildResult();
    ctx->result->cost = 0;
    ctx->result->nextWorkerId = 0;
    
    /* add builtin types */

    ctx->typeTable["i8"] = new TypeContext(1, TYPE_SCALAR, {._scalar={SCALAR_I8}});
    ctx->typeTable["i16"] = new TypeContext(2, TYPE_SCALAR, {._scalar={SCALAR_I16}});
    ctx->typeTable["i32"] = new TypeContext(4, TYPE_SCALAR, {._scalar={SCALAR_I32}});
    ctx->typeTable["i64"] = new TypeContext(8, TYPE_SCALAR, {._scalar={SCALAR_I64}});

    ctx->typeTable["u8"] = new TypeContext(1, TYPE_SCALAR, {._scalar={SCALAR_U8}});
    ctx->typeTable["u16"] = new TypeContext(2, TYPE_SCALAR, {._scalar={SCALAR_U16}});
    ctx->typeTable["u32"] = new TypeContext(4, TYPE_SCALAR, {._scalar={SCALAR_U32}});
    ctx->typeTable["u64"] = new TypeContext(8, TYPE_SCALAR, {._scalar={SCALAR_U64}});
    
    ctx->typeTable["f32"] = new TypeContext(4, TYPE_SCALAR, {._scalar={SCALAR_F32}});
    ctx->typeTable["f64"] = new TypeContext(8, TYPE_SCALAR, {._scalar={SCALAR_F64}});

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


pair<BuildResult *, bool> buildAst(const char *filename, char *source, vector<Node *>nodes, map<string, string> configs)
{
    /* compress AST tree */
    for (auto &node : nodes)
    {
        node = compressNode(node);
        dumpAst(node);
    }
    BuildContext *ctx = initializateContext(filename, source, configs);
    for (auto &node : nodes)
    {
        assert_type(node, "Global");
        assert(node->childs.size() == 1);

        if (is(node->childs[0], "S"))
        {
            /* skip empty node */
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
            printf("Unknown global node type: <%s>\n", node->childs[0]->rule->name);
            logError(filename, source, node->childs[0]->start);
        }
    }
    for (auto &node : nodes)
    {
        assert_type(node, "Global");
        assert(node->childs.size() == 1);

        if (is(node->childs[0], "worker"))
        {
            Node *code = node->childs[0]->nonTerm(4);
            WorkerDeclarationContext *wk = getWorkerByName(ctx, Substr(ctx, node->childs[0]->nonTerm(2)));
            buildWorkerContent(ctx, wk, code);
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
        case OP_LOAD:
        case OP_NEW_INT:
        case OP_NEW_FLOAT:
            T(op->data[0]);
            break;
        
        // all args
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
    
    #undef T
}

vector<int64_t> getWritedVariables(OperationBlock *op)
{    
    switch (op->type)
    {   
        // 2nd
        case OP_LOAD_INPUT:
        case OP_LOAD_OUTPUT:
            return {op->data[1]};
            
        case OP_CALL:
        case OP_FREE_TEMP:
        case OP_PUSH_PIPE:
        case OP_PUSH_PROMISE:
        case OP_PUSH_CLASS:
        case OP_PUSH_ARRAY:
        case OP_JMP:
        case OP_JZ:
        case OP_JNZ:
        case OP_STORE:
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
    switch (op->type)
    {
        // 2nd
        case OP_LOAD_INPUT:
        case OP_LOAD_OUTPUT:
            return {op->data[1]};

        // 1, last
        case OP_PUSH_VAR:
        case OP_PUSH_CLASS:
            return {op->data[0], op->data.back()};

        // 1, 2
        case OP_QUERY_VAR:
        case OP_QUERY_CLASS:
            return {op->data[0], op->data[1]};

        // 1, 2 and last
        case OP_PUSH_ARRAY:
        case OP_QUERY_INDEX:
            return {op->data[0], op->data[1], op->data.back()};
            
        // all except first
        case OP_CALL:
        {
            auto t = op->data | views::drop(1);
            return vector<int64_t>(t.begin(), t.end());
        }

        // first arg
        case OP_STORE:
        case OP_LOAD:
        case OP_NEW_CLASS:
        case OP_NEW_INT:
        case OP_NEW_FLOAT:
            return {op->data[0]};
        
        // all args
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
            return op->data;
    }
}

vector<int64_t> getReadVariables(OperationBlock *op)
{    
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
        case OP_NEW_PIPE:
        case OP_NEW_PROMISE:
            return {};

        // first and last
        case OP_PUSH_PIPE:
        case OP_PUSH_PROMISE:
        case OP_PUSH_CLASS:
            return {op->data[0], op->data.back()};
        
        // last
        case OP_PUSH_VAR:
            return {op->data.back()};
            
        // 2
        case OP_QUERY_VAR:
        case OP_QUERY_CLASS:
            return {op->data[1]};

        // 1, 2 and last
        case OP_PUSH_ARRAY:
            return {op->data[0], op->data[1], op->data.back()};

        // 2 and last
        case OP_QUERY_INDEX:
            return {op->data[1], op->data.back()};
            

        // first arg
        case OP_JZ:
        case OP_JNZ:
        case OP_STORE:
            return {op->data[0]};
        
        
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
            return vector<int64_t>(t.begin(), t.end());
        }
    }
}

void dumpIR(WorkerDeclarationContext *fn)
{
    printf("Worker %s\n", fn->name.c_str());
    if (fn->content)
    {
        printf("code:\n");
        for (auto &x : fn->content->code)
        {
            printf("%p ", x);
            switch (x->type)
            {
            
                case OP_LOAD_INPUT: printf("OP_LOAD_INPUT "); break;
                case OP_LOAD_OUTPUT: printf("OP_LOAD_OUTPUT "); break;
                
                case OP_FREE_TEMP: printf("OP_FREE_TEMP "); break;
                
                case OP_STORE: printf("OP_STORE "); break;
                case OP_LOAD: printf("OP_LOAD "); break;
                
                case OP_CALL: printf("OP_CALL "); break;
                case OP_CAST: printf("OP_CAST "); break;
                case OP_MOV: printf("OP_MOV "); break;

                case OP_NEW_INT: printf("OP_NEW_INT "); break;
                case OP_NEW_FLOAT: printf("OP_NEW_FLOAT "); break;
                case OP_NEW_ARRAY: printf("OP_NEW_ARRAY "); break;
                case OP_NEW_PIPE: printf("OP_NEW_PIPE "); break;
                case OP_NEW_PROMISE: printf("OP_NEW_PROMISE "); break;
                case OP_NEW_CLASS: printf("OP_NEW_CLASS "); break;
                
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
                    printf("%s=%s ", k.c_str(), v.c_str());
                }
                printf("}");
            }
            printf("\n");
        }
    }
}
