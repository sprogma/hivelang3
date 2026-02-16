#include "../../ir.hpp"
#include "../utils.hpp"


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

