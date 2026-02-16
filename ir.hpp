#ifndef IR_HPP
#define IR_HPP

#include <vector>
#include <string>
#include <map>
#include <set>

using namespace std;

#include "ast.hpp"
#include "utils.hpp"

typedef uint8_t BYTE;


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
    string provider;
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

inline constexpr auto IS_LINK_TYPE = is_one<TYPE_CLASS, TYPE_ARRAY, TYPE_PIPE, TYPE_PROMISE>;

enum OperationType
{
    OP_LOAD_INPUT,
    OP_LOAD_OUTPUT,
    OP_FREE_TEMP,
    
    OP_CALL,
    OP_CAST,
    
    OP_MOV,
    
    OP_NEW_INT, // format is [dest, value]
    OP_NEW_FLOAT, // format is [dest, value[int64_t encoded]]
    OP_NEW_ARRAY, // format is [dest, length]
    OP_NEW_PIPE, // format is [dest]
    OP_NEW_PROMISE, // format is [dest]
    OP_NEW_CLASS, // format is [dest]
    OP_NEW_STRING, // format is [dest, id] id is from global string buffer

    // PUSH_VAR is used only with structures - owerwise MOV is generated
    OP_PUSH_VAR, // format is [var, offset, type?, source]
    OP_PUSH_ARRAY, // format is [array, index, offset, type?, source]
    OP_PUSH_PIPE, // format is [pipe, source]
    OP_PUSH_PROMISE, // format is [promise, source]
    OP_PUSH_CLASS, // format is [class, offset, type?, source]
    
    // QUERY_VAR is used only with structures - owerwise MOV is generated
    OP_QUERY_VAR, // format is [dest, var, offset, type?]
    OP_QUERY_ARRAY, // format is [dest, source]
    OP_QUERY_INDEX, // format is [dest, source, offset, type?, index]
    OP_QUERY_PIPE, // format is [dest, source]
    OP_QUERY_PROMISE, // format is [dest, source]
    OP_QUERY_CLASS, // format is [dest, source, offset, type?]
    
    // format [time:i64 [TODO: float sleep]] [in ms?]
    OP_SLEEP,
    
    OP_JZ, OP_JNZ,
    OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_MOD,
    OP_BOR, OP_BAND, OP_BXOR, OP_SHL, OP_SHR, OP_BNOT,
    OP_EQ, OP_LT, OP_LE, OP_GT, OP_GE, OP_NE,

    // used for register allocations
    OP_STORE, // format is [variable, place]
    OP_LOAD, // format is [variable, place]
    // used for generating input table, to call using it
    OP_STORE_INPUT, // format is [variable, offset]

    // used only in parsing - don't use it.
    OP_JMP,
};

inline constexpr auto IS_JUMP = is_one<OP_JMP, OP_JZ, OP_JNZ>;


struct OperationBlock
{
    OperationType type;
    vector<int64_t> data;
    map<string, variant<string, int64_t>> attributes = {};
    vector<OperationBlock *> next;
    set<OperationBlock *> prev;
    
    int64_t code_start, code_end;
};


// defined worker
struct WorkerContext
{
    OperationBlock *entry;
    vector<OperationBlock *> code;
    map<int64_t, TypeContext *>variables;
    int64_t nextVarId;
    int64_t nextTempId;
};


// any worker
struct WorkerDeclarationContext
{
    string name;
    map<string, variant<string, int64_t>> attributes;
    vector<pair<string, TypeContext *>> inputs;
    vector<pair<string, TypeContext *>> outputs;
    WorkerContext *content;

    int64_t code_start, code_end, code_block_end;

    set<string> used_providers;

    ~WorkerDeclarationContext()
    {
        delete content;
    }
};


struct BuildResult
{
    const char *filename;
    char *code;
    
    double cost;
    map<WorkerDeclarationContext *, int64_t> workers;
    map<string, int64_t> names;
    vector<vector<BYTE>> strings;
    int64_t nextWorkerId;
    map<pair<int64_t, string>, int64_t> exportWorkerId;

    set<string> used_providers;

    ~BuildResult()
    {
        for (auto i : workers)
        {
            delete i.first;
        }
    }
};

#define FIRST_TEMP_ID 1000000

struct BuildContext
{
    map<string, string> configs;
    
    const char *filename;
    char *code;
    map<pair<string, string>, TypeContext *> typeTable;
    vector<TypeContext *> types;
    map<string, int64_t> names;
    map<int64_t, TypeContext *> variables;
    WorkerDeclarationContext *current;
    int64_t nextVarId;
    int64_t nextTempId;
    BuildResult *result;

    string provider;
    string entryProvider;
    
    ~BuildContext() {
        for (auto &p : typeTable) delete p.second;
        typeTable.clear();
    }

    bool enabled(string x)
    {
        return configs.find(x) != configs.end();
    }
};


/* - api */
pair<BuildResult *, bool> buildAst(const char *filename, char *source, vector<Node *>nodes, map<string, string> configs, string entryProvider);

vector<int64_t> getWritedVariables(OperationBlock *op);
vector<int64_t> getReadVariables(OperationBlock *op);
vector<int64_t> getUsedVariables(OperationBlock *op); // all variables from operation
void applyNamesTranslition(OperationBlock *block, const map<int64_t, int64_t> &translition);

void dumpIR(WorkerDeclarationContext *worker);

int64_t ExportCode(BuildResult *code);

#endif
