// this sourse file was generated using grammar.fsx
#include <vector>
#include <map>
using namespace std;
#include "ast.hpp"
Rule *grammar;
Rule *generateGrammar()
{
// allocate memory for all elements
Rule *array = (Rule *)malloc(sizeof(*array) * 23);
new(array + 1)Rule(1, "S", vector<RuleVariant>{RuleVariant(vector<Atom>{grammar_fn_spaces})});
new(array + 2)Rule(2, "Sn", vector<RuleVariant>{RuleVariant(vector<Atom>{grammar_fn_spaces_or_no})});
new(array + 3)Rule(3, "identifer", vector<RuleVariant>{RuleVariant(vector<Atom>{grammar_fn_identifer})});
new(array + 4)Rule(4, "integer", vector<RuleVariant>{RuleVariant(vector<Atom>{grammar_fn_integer})});
new(array + 5)Rule(5, "float", vector<RuleVariant>{RuleVariant(vector<Atom>{grammar_fn_float})});
new(array + 20)Rule(20, "Expr", vector<RuleVariant>{RuleVariant(vector<Atom>{"a", array + 21, "b"}, vector<Atom>{"a", array + 21, "b"}, vector<Atom>{"a", array + 21, "b"})});
new(array + 21)Rule(21, "Term", vector<RuleVariant>{RuleVariant(vector<Atom>{"t"}, vector<Atom>{"t"}, vector<Atom>{"t"}), RuleVariant(vector<Atom>{"v"}, vector<Atom>{"v"}, vector<Atom>{"v"})});
return array;
}