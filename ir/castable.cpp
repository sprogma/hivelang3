#include "utils.hpp"
#include "../ir.hpp"


// from t2 to t1
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

// from t2 to t1
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
    if (t1->type == TYPE_CLASS && t2->type == TYPE_CLASS)
    {
        // TODO: make something more clever
        return true; // all classes can be castable too
    }
    if (t1->type == t2->type && t1->size == t2->size)
    {
        // TODO: move check to providers instead of confirming all
        switch (t1->type)
        {
            case TYPE_CLASS:
            case TYPE_UNION:
            case TYPE_RECORD:
                return t1->_struct.fields == t2->_struct.fields &&
                       t1->_struct.names == t2->_struct.names;
            case TYPE_SCALAR:
                return t1->_scalar.kind == t2->_scalar.kind;
            case TYPE_ARRAY:
            case TYPE_PIPE:
            case TYPE_PROMISE:
                return t1->_vector.base == t2->_vector.base;
        }
    }
    return false;
}

