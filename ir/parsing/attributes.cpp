#include "../../ir.hpp"
#include "../utils.hpp"


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

