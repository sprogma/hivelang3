#ifndef AST_HPP
#define AST_HPP

#include <assert.h>
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
    bool pass;
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
    vector<Node *> childs;

    Node *nonTerm(int64_t index)
    {
        for (auto i : childs)
        {
            // TODO: move 3 to const
            if (i->rule && i->rule->id >= 3)
            {
                if (!index--)
                {
                    return i;
                }
            }
        }
        return NULL;
    }
};


pair<Node *, int64_t>grammar_fn_spaces(char *content, int64_t position);
pair<Node *, int64_t>grammar_fn_spaces_or_no(char *content, int64_t position);
pair<Node *, int64_t>grammar_fn_identifer(char *content, int64_t position);
pair<Node *, int64_t>grammar_fn_identifer_or_number(char *content, int64_t position);
pair<Node *, int64_t>grammar_fn_integer(char *content, int64_t position);
pair<Node *, int64_t>grammar_fn_float(char *content, int64_t position);

Rule *grammarGetRule(const char *name);


extern Rule *grammar;
extern int64_t grammar_len;


Rule *generateGrammar();
pair<vector<Node *>, bool>parse(const char *filename, Rule *baseRule, char *content);
void dumpAst(Node *x, int t = 0);

#endif
