#include "../../ir.hpp"
#include "../utils.hpp"



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

