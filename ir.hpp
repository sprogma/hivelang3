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


#define SCALAR_F 0x1000
#define SCALAR_I 0x2000
#define SCALAR_U 0x4000
#define SCALAR_TYPE(x) ((x) & 0xF000)


enum ScalarInterpretation
{
    SCALAR_F32 = SCALAR_F | 0,
    SCALAR_F64 = SCALAR_F | 1,
    SCALAR_I8 = SCALAR_I | 0,
    SCALAR_I16 = SCALAR_I | 1,
    SCALAR_I32 = SCALAR_I | 2,
    SCALAR_I64 = SCALAR_I | 3,
    SCALAR_U8 = SCALAR_U | 0,
    SCALAR_U16 = SCALAR_U | 1,
    SCALAR_U32 = SCALAR_U | 2,
    SCALAR_U64 = SCALAR_U | 3,
};


struct TypeContext
{
    int64_t size;
    TypeContextType type;
    union
    {
        struct
        {
            vector<TypeContext *> fields;
            map<string, int64_t> names;
        } _struct;
        struct
        {
            TypeContext *base;
        } _vector;
        struct
        {
            ScalarInterpretation kind;
        } _scalar;
    };

    ~TypeContext()
    {
        switch (type)
        {
            case TYPE_CLASS:
            case TYPE_UNION:
            case TYPE_RECORD:
                _struct.fields.clear();
                _struct.names.clear();
                break;
            case TYPE_ARRAY:
            case TYPE_PIPE:
            case TYPE_PROMISE:
            case TYPE_SCALAR:
                break;
        }
    }
};


enum OperationType
{
    OP_LOAD_INPUT,
    OP_LOAD_OUTPUT,
    OP_FREE_TEMP,
    OP_NEW, OP_QUERY, OP_PUSH,
    OP_JMP, OP_JZ, OP_JNZ,
    OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_MOD,
    OP_EQ, OP_LT, OP_LE, OP_GT, OP_GE,
};


struct Operation
{
    OperationType type;
    int64_t *data;
};


// defined worker
struct WorkerContext
{
    vector<Operation> code;
};


// any worker
struct WorkerDeclarationContext
{
    string name;
    map<string, string> attributes;
    vector<pair<string, TypeContext *>> inputs;
    vector<pair<string, TypeContext *>> outputs;
    WorkerContext *content;
};


struct BuildResult
{
    vector<WorkerDeclarationContext *> workers;
};


struct BuildContext
{
    const char *filename;
    char *code;
    map<string, TypeContext *> typeTable;
    vector<TypeContext *> types;
    map<string, int64_t> names;
    map<int64_t, TypeContext *> variables;
    map<int64_t, TypeContext *> temporaries;
    int64_t nextVarId;
    int64_t nextTempId;
    BuildResult *result;
    
    ~BuildContext() {
        for (auto &p : typeTable) delete p.second;
        typeTable.clear();
    }
};


/* - api */
pair<BuildResult *, bool> buildAst(const char *filename, char *code, vector<Node *>nodes);

#endif
