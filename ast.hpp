#ifndef AST_HPP
#define AST_HPP

#include <variant>
#include <vector>
#include <inttypes.h>

using namespace std;

struct Node;
struct Rule;
struct RuleVariant;

using Atom = variant<const char *, Rule *, pair<Node *, int64_t>(*)(char *, int64_t)>;

struct RuleVariant
{
    vector<Atom> prefix;
    vector<Atom> period;
    vector<Atom> suffix;
};

struct Rule
{
    int64_t id;
    const char *name;
    vector<RuleVariant> variants; 
};

struct Node
{
    Rule *rule;
    int64_t variant;
    int64_t start, end;
    int64_t childs_len;
    Node **childs;
};


pair<Node *, int64_t>grammar_fn_spaces(char *content, int64_t position);
pair<Node *, int64_t>grammar_fn_spaces_or_no(char *content, int64_t position);
pair<Node *, int64_t>grammar_fn_identifer(char *content, int64_t position);
pair<Node *, int64_t>grammar_fn_integer(char *content, int64_t position);
pair<Node *, int64_t>grammar_fn_float(char *content, int64_t position);


extern Rule *grammar;


Rule *generateGrammar();
pair<vector<Node *>, bool>parse(Rule *baseRule, char *content);

#endif
