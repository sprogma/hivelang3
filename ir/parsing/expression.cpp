#include "../../ir.hpp"
#include "../utils.hpp"


pair<vector<Operation>, int64_t> buildExpression(BuildContext *ctx, Node *node)
{
    assert_type(node, "expression");
    return buildSetOperation(ctx, node->nonTerm(0));
}
