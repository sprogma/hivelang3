#ifndef IR_HPP
#define IR_HPP

#include <vector>
#include <string>
#include <map>

using namespace std;

#include "ast.hpp"


enum TypeContextType
{
    TYPE_UNION,
    TYPE_RECORD,
    TYPE_CLASS,
    TYPE_PIPE,
    TYPE_ARRAY,
    TYPE_PROMISE,
    TYPE_SCALAR,
};


struct TypeContext
{
    TypeContextType type;
    union
    {
        struct
        {
            map<string, TypeContext> fields;
        } _union;
        struct
        {
            map<string, TypeContext> fields;
        } _struct;
        struct
        {
            map<string, TypeContext> fields;
        } _struct;
    };
};


struct BuildContext
{
    map<string, TypeContext>
};


struct Worker
{
    vector<int64_t> code;
};

pair<vector<Worker *>, bool> buildAst(const char *filename, char *code, vector<Node *>nodes);

#endif
