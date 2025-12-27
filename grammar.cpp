// this sourse file was generated using grammar.fsx
#include <vector>
#include <map>
using namespace std;
#include "ast.hpp"
Rule *grammar;
int64_t grammar_len;
Rule *generateGrammar()
{
// allocate memory for all elements
grammar_len = 24;
Rule *array = (Rule *)malloc(sizeof(*array) * grammar_len);
new(array + 1)Rule(1, "S", vector<RuleVariant>{RuleVariant(vector<Atom>{grammar_fn_spaces})});
new(array + 2)Rule(2, "Sn", vector<RuleVariant>{RuleVariant(vector<Atom>{grammar_fn_spaces_or_no})});
new(array + 3)Rule(3, "identifer", vector<RuleVariant>{RuleVariant(vector<Atom>{grammar_fn_identifer})});
new(array + 4)Rule(4, "integer", vector<RuleVariant>{RuleVariant(vector<Atom>{grammar_fn_integer})});
new(array + 5)Rule(5, "float", vector<RuleVariant>{RuleVariant(vector<Atom>{grammar_fn_float})});
new(array + 20)Rule(20, "Expr", vector<RuleVariant>{RuleVariant(vector<Atom>{"a", array + 22, "b"}, vector<Atom>{}, vector<Atom>{})});
new(array + 21)Rule(21, "Global", vector<RuleVariant>{RuleVariant(vector<Atom>{array + 1}, vector<Atom>{}, vector<Atom>{}), RuleVariant(vector<Atom>{array + 20}, vector<Atom>{}, vector<Atom>{})});
new(array + 22)Rule(22, "Term", vector<RuleVariant>{RuleVariant(vector<Atom>{"t"}, vector<Atom>{}, vector<Atom>{}), RuleVariant(vector<Atom>{"v"}, vector<Atom>{}, vector<Atom>{})});
return array;
}