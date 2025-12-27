#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <map>
#include <string>
#include <vector>
#include <variant>
#include <algorithm>

using namespace std;

#include "logger.hpp"
#include "ast.hpp"
#include "ir.hpp"

#define is(node, str) (node->rule && strcmp(node->rule->name, str) == 0)

#define assert_type(node, str) assert(is(node, str))
#define switch_type(x) switch (x->rule ? x->rule->id : 0) 
#define switch_var(x) switch (x->variant)


template<typename... Args>
int64_t* data(Args... args) 
{
    constexpr size_t N = sizeof...(Args);
    int64_t* arr = new int64_t[N];
    int64_t temp[] = { (int64_t)(args)... };
    memcpy(arr, temp, sizeof(*arr) * N);
    return arr;
}

vector<Operation> buildCodeBlock(BuildContext *ctx, Node *node);

string Substr(BuildContext *ctx, Node *node)
{
    return string(ctx->code + node->start, node->end - node->start);
}

WorkerDeclarationContext *getWorkerByName(BuildContext *ctx, string name)
{
    for (auto &x : ctx->result->workers)
    {
        if (x->name == name)
        {
            return x;
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
    ctx->temporaries[ctx->nextTempId] = type;
    return ctx->nextTempId++;
}

void freeTemp(vector<Operation> &code, int64_t id)
{
    if (id != -1)
    {
        code.push_back({OP_FREE_TEMP, data(id)});
    }
}

pair<vector<Operation>, int64_t> buildExpression(BuildContext *ctx, Node *node)
{
    (void)ctx;
    assert_type(node, "expression");
    return {};
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
            ops.insert(ops.end(), expr.begin(), expr.end());
            freeTemp(ops, exprPos);
            break;
        }
        case 2:
        {
            printf("statement while\n");
            auto [guard, guardPos] = buildExpression(ctx, node->nonTerm(0)); 
            auto body = buildCodeBlock(ctx, node->nonTerm(1));
            ops.push_back({OP_JMP, data(1 + body.size())});
            ops.insert(ops.end(), body.begin(), body.end());
            ops.insert(ops.end(), guard.begin(), guard.end());
            ops.push_back({OP_JNZ, data(- guard.size() - body.size(), guardPos)});
            freeTemp(ops, guardPos);
            break;
        }
        case 3:
        {    
            printf("statement match\n");
            auto [match, matchPos] = buildExpression(ctx, node->nonTerm(0));
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
                            auto block = buildCodeBlock(ctx, var->nonTerm(1));
                            int64_t temp = ctx->nextTempId++;
                            ops.insert(ops.end(), pattern.begin(), pattern.end());
                            ops.push_back({OP_EQ, data(temp, matchPos, patternPos)});
                            freeTemp(ops, patternPos);
                            ops.push_back({OP_JNZ, data(1 + block.size(), temp)});
                            freeTemp(ops, temp);
                            ops.insert(ops.end(), block.begin(), block.end());
                            break;
                        }
                    }
                }
            }
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
        ops.insert(ops.end(), res.begin(), res.end());
        id++;
    }
    return ops;
}

void buildWorkerContent(BuildContext *ctx, WorkerDeclarationContext *wk, Node *node)
{
    assert_type(node, "code_block");
    printf("Building worker %s ...\n", wk->name.c_str());
    
    ctx->nextVarId = 0;
    ctx->nextTempId = 100000;
    ctx->names.clear();
    ctx->variables.clear();
    ctx->temporaries.clear();
    
    wk->content = new WorkerContext();
    /* create variables for inputs + code to load them */
    int64_t inputId = 0;
    for (auto &[name, type] : wk->inputs)
    {
        int64_t varId = ctx->names[name] = ctx->nextVarId++;
        ctx->variables[varId] = type;
        wk->content->code.push_back({OP_LOAD_INPUT, data(inputId++, varId)});
    }
    /* create variables for outputs? */
    int64_t outputId = 0;
    for (auto &[name, type] : wk->outputs)
    {
        int64_t varId = ctx->names[name] = ctx->nextVarId++;
        ctx->variables[varId] = type;
        wk->content->code.push_back({OP_LOAD_OUTPUT, data(outputId++, varId)});
    }
    /* build body */
    auto res = buildCodeBlock(ctx, node);
    wk->content->code.insert(wk->content->code.end(), res.begin(), res.end());
}


void addWorkerDefinition(BuildContext *ctx, Node *node, bool with_code)
{
    if (with_code) { assert_type(node, "worker"); }
    else { assert_type(node, "worker_decl"); }
    
    WorkerDeclarationContext *wk = new WorkerDeclarationContext();

    int64_t attrId = 0;
    Node *attrList = node->nonTerm(0);
    assert_type(attrList, "attribute_list");
    while (attrList->nonTerm(attrId))
    {
        Node *key = attrList->nonTerm(attrId + 0);
        Node *value = attrList->nonTerm(attrId + 1);
        assert_type(key, "identifer_or_number");
        assert_type(value, "identifer_or_number");
        wk->attributes[Substr(ctx, key)] = Substr(ctx, value);
        attrId += 2;
    }

    printf("read %lld attributes\n", attrId / 2);

    /* read arguments */
    wk->inputs = readWorkerArgList(ctx, node->nonTerm(1));
    wk->outputs = readWorkerArgList(ctx, node->nonTerm(3));
    
    wk->name = Substr(ctx, node->nonTerm(2));
    
    printf("Worker %s declaration generated\n", wk->name.data());

    ctx->result->workers.push_back(wk);
}

BuildContext *initializateContext(const char *filename, char *source)
{
    BuildContext *ctx = new BuildContext(filename, source);
    ctx->result = new BuildResult();
    
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

pair<BuildResult *, bool> buildAst(const char *filename, char *source, vector<Node *>nodes)
{
    BuildContext *ctx = initializateContext(filename, source);
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
    // delete ctx;
    return {ctx->result, false};
}
