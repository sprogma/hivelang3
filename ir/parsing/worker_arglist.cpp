#include "../../ir.hpp"
#include "../utils.hpp"


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
