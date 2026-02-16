#include "../utils.hpp"
#include "../../ir.hpp"


void registerStructure(BuildContext *ctx, TypeContextType type, Node *node)
{
    switch (type)
    {
        case TYPE_RECORD: assert_type(node, "_record"); break;
        case TYPE_UNION: assert_type(node, "_union"); break;
        case TYPE_CLASS: assert_type(node, "_class"); break;
        default:
            logError(ctx->filename, ctx->code, node->start, node->end, "registerStructure called with not TYPE_RECORD/TYPE_UNION/TYPE_CLASS");
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

