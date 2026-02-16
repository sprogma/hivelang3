#include "utils.hpp"
#include "../ir.hpp"


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

