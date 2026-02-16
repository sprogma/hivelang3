#ifndef IR_UTILS
#define IR_UTILS

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <map>
#include <string>
#include <vector>
#include <ranges>
#include <variant>
#include <algorithm>

using namespace std;

#include "../optimization/optimizer.hpp"
#include "../codegen/codegen.hpp"
#include "../logger.hpp"
#include "../ast.hpp"
#include "../ir.hpp"

/* pre-intermediate representation */
struct Operation
{
    OperationType type;
    vector<int64_t> data;
    map<string, variant<string, int64_t>> attributes = {};

    int64_t code_start, code_end;
};

#define is(node, str) (node->rule && strcmp(node->rule->name, str) == 0)
#define assert_type(node, str) do{if (!is((node), str)){if((node)->rule){printf("[Have <%s> instead]\n", (node)->rule->name);}assert(is((node), str));}}while(0)
#define switch_type(x) switch ((x)->rule ? (x)->rule->id : 0)
#define switch_var(x) switch ((x)->variant)


void applyNamesTranslition(OperationBlock *op, const map<int64_t, int64_t> &translition);
vector<int64_t> getReadVariables(OperationBlock *op);
vector<int64_t> getUsedVariables(OperationBlock *op);
vector<int64_t> getWritedVariables(OperationBlock *op);


string printType(TypeContext *t);
void dumpIRR(WorkerDeclarationContext *fn, OperationBlock *x);
void dumpIR(WorkerDeclarationContext *fn);


#define HANDLE_NOT_NULL(tmp, node) \
    if (handleNotNull(ctx, tmp, node)) return {{}, -1};

bool handleNotNull(BuildContext *ctx, int64_t tmp, Node *node);
string Substr(BuildContext *ctx, Node *node);
bool validateProviderWithError(BuildContext *ctx, Node *node, const string &name);
void freeAttributeTemps(vector<Operation> &ops, map<string, variant<string, int64_t>> attributes);
int64_t append(vector<Operation> &a, Operation b);
void append(vector<Operation> &a, vector<Operation> &b);
void freeTemp(vector<Operation> &code, int64_t id);
int64_t newTemp(BuildContext *ctx, TypeContext *type);
int64_t GetStringId(BuildContext *ctx, const vector<BYTE> &value);
WorkerDeclarationContext *getWorkerByName(BuildContext *ctx, string name);
pair<int64_t, TypeContext *> GetFieldOffset(BuildContext *ctx, TypeContext *type, int64_t field);
pair<int64_t, TypeContext *> GetFieldOffset(BuildContext *ctx, Node *node, int64_t fromId, TypeContext *type);


bool is_convertable(TypeContext *to, TypeContext *from);
bool is_castable(TypeContext *to, TypeContext *from);


pair<bool, TypeContext *> operation_types(TypeContext *t1, TypeContext *t2);


TypeContext *getDerivative(BuildContext *ctx, TypeContext *type, TypeContextType derivative, const string &provider="");
TypeContext *getBaseType(BuildContext *ctx, const string &name, const string &provider="");
TypeContext *getType(BuildContext *ctx, Node *node);
TypeContext *getIntegerType(BuildContext *ctx, int64_t x);

void registerStructure(BuildContext *ctx, TypeContextType type, Node *node);



// parsing functions
pair<map<string, variant<string, int64_t>>, vector<Operation>> getAttributeList(BuildContext *ctx, Node *node, bool support_expression);
vector<pair<string, TypeContext *>> readWorkerArgList(BuildContext *ctx, Node *node);
pair<vector<Operation>, int64_t> buildExpression(BuildContext *ctx, Node *node);
pair<vector<Operation>, int64_t> buildSimpleTerm(BuildContext *ctx, Node *node);
pair<vector<Operation>, int64_t> buildQueryOperation(BuildContext *ctx, Node *node);
pair<vector<Operation>, int64_t> buildIndexOperation(BuildContext *ctx, Node *node);
pair<vector<Operation>, int64_t> buildPrefixOperation(BuildContext *ctx, Node *node);
pair<vector<Operation>, int64_t> buildBinOperation(BuildContext *ctx, Node *node);
pair<vector<Operation>, int64_t> buildMulOperation(BuildContext *ctx, Node *node);
pair<vector<Operation>, int64_t> buildAddOperation(BuildContext *ctx, Node *node);
pair<vector<Operation>, int64_t> buildCompareOperation(BuildContext *ctx, Node *node);
pair<vector<Operation>, int64_t> buildLogicOperation(BuildContext *ctx, Node *node);
pair<vector<Operation>, int64_t> buildSetOperation(BuildContext *ctx, Node *node);
pair<vector<Operation>, int64_t> buildExpression(BuildContext *ctx, Node *node);
vector<Operation> buildStatement(BuildContext *ctx, Node *node);
vector<Operation> buildCodeBlock(BuildContext *ctx, Node *node);


#endif
