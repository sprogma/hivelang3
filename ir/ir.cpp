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

#include "utils.hpp"
#include "../optimization/optimizer.hpp"
#include "../codegen/codegen.hpp"
#include "../logger.hpp"
#include "../ast.hpp"
#include "../ir.hpp"


pair<vector<OperationBlock *>, map<OperationBlock *, int64_t>> OpsToBlocks(const vector<Operation> &ops)
{
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
    return {data, blockId};
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
    auto [data, blockId] = OpsToBlocks(ops);
    
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
    /* convert it to inter-state */
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
            registerStructure(ctx, TYPE_RECORD, node->childs[0]);
        }
        else if (is(node->childs[0], "_union"))
        {
            registerStructure(ctx, TYPE_UNION, node->childs[0]);
        }
        else if (is(node->childs[0], "_class"))
        {
            registerStructure(ctx, TYPE_CLASS, node->childs[0]);
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

