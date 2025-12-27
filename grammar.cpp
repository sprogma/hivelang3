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
grammar_len = 50;
Rule *array = (Rule *)malloc(sizeof(*array) * grammar_len);
new(array + 1)Rule(1, "S", vector<RuleVariant>{RuleVariant(vector<Atom>{grammar_fn_spaces})});
new(array + 2)Rule(2, "Sn", vector<RuleVariant>{RuleVariant(vector<Atom>{grammar_fn_spaces_or_no})});
new(array + 3)Rule(3, "identifer", vector<RuleVariant>{RuleVariant(vector<Atom>{grammar_fn_identifer})});
new(array + 4)Rule(4, "identifer_or_number", vector<RuleVariant>{RuleVariant(vector<Atom>{grammar_fn_identifer_or_number})});
new(array + 5)Rule(5, "integer", vector<RuleVariant>{RuleVariant(vector<Atom>{grammar_fn_integer})});
new(array + 6)Rule(6, "float", vector<RuleVariant>{RuleVariant(vector<Atom>{grammar_fn_float})});
new(array + 20)Rule(20, "AddOperation", vector<RuleVariant>{RuleVariant(vector<Atom>{array + 24, array + 31}, vector<Atom>{array + 24, array + 31}, vector<Atom>{array + 24}), RuleVariant(vector<Atom>{array + 24}, vector<Atom>{}, vector<Atom>{})});
new(array + 21)Rule(21, "CompareOperation", vector<RuleVariant>{RuleVariant(vector<Atom>{array + 20, array + 37}, vector<Atom>{array + 20, array + 37}, vector<Atom>{array + 20}), RuleVariant(vector<Atom>{array + 20}, vector<Atom>{}, vector<Atom>{})});
new(array + 22)Rule(22, "Global", vector<RuleVariant>{RuleVariant(vector<Atom>{array + 1}, vector<Atom>{}, vector<Atom>{}), RuleVariant(vector<Atom>{array + 29}, vector<Atom>{}, vector<Atom>{}), RuleVariant(vector<Atom>{array + 30}, vector<Atom>{}, vector<Atom>{}), RuleVariant(vector<Atom>{array + 28}, vector<Atom>{}, vector<Atom>{}), RuleVariant(vector<Atom>{array + 47}, vector<Atom>{}, vector<Atom>{}), RuleVariant(vector<Atom>{array + 48}, vector<Atom>{}, vector<Atom>{})});
new(array + 23)Rule(23, "IndexOperation", vector<RuleVariant>{RuleVariant(vector<Atom>{array + 27, array + 2, "[", array + 39, array + 2, "]"}, vector<Atom>{}, vector<Atom>{}), RuleVariant(vector<Atom>{array + 27}, vector<Atom>{}, vector<Atom>{})});
new(array + 24)Rule(24, "MulOperation", vector<RuleVariant>{RuleVariant(vector<Atom>{array + 25, array + 40}, vector<Atom>{array + 25, array + 40}, vector<Atom>{array + 25}), RuleVariant(vector<Atom>{array + 25}, vector<Atom>{}, vector<Atom>{})});
new(array + 25)Rule(25, "QueryOperation", vector<RuleVariant>{RuleVariant(vector<Atom>{array + 2, "?", array + 23}, vector<Atom>{}, vector<Atom>{}), RuleVariant(vector<Atom>{array + 23, array + 2, "?", array + 2, array + 23}, vector<Atom>{}, vector<Atom>{}), RuleVariant(vector<Atom>{array + 23}, vector<Atom>{}, vector<Atom>{})});
new(array + 26)Rule(26, "SetOperation", vector<RuleVariant>{RuleVariant(vector<Atom>{array + 23, array + 2, "<-"}, vector<Atom>{array + 23, array + 2, "<-"}, vector<Atom>{array + 21}), RuleVariant(vector<Atom>{array + 21}, vector<Atom>{}, vector<Atom>{})});
new(array + 27)Rule(27, "SimpleTerm", vector<RuleVariant>{RuleVariant(vector<Atom>{array + 2, array + 41}, vector<Atom>{}, vector<Atom>{}), RuleVariant(vector<Atom>{array + 2, array + 6}, vector<Atom>{}, vector<Atom>{}), RuleVariant(vector<Atom>{array + 2, array + 5}, vector<Atom>{}, vector<Atom>{}), RuleVariant(vector<Atom>{array + 2, array + 3}, vector<Atom>{".", array + 3}, vector<Atom>{}), RuleVariant(vector<Atom>{array + 2, "(", array + 34, array + 2, ")", array + 2, array + 3, array + 33, array + 2, "(", array + 35, array + 2, ")"}, vector<Atom>{}, vector<Atom>{}), RuleVariant(vector<Atom>{array + 2, "(", array + 39, array + 2, ")"}, vector<Atom>{}, vector<Atom>{})});
new(array + 28)Rule(28, "_class", vector<RuleVariant>{RuleVariant(vector<Atom>{array + 2, "class", array + 1, array + 3, array + 2, "{"}, vector<Atom>{array + 45, array + 1, array + 3, array + 2, ";"}, vector<Atom>{array + 2, "}"})});
new(array + 29)Rule(29, "_record", vector<RuleVariant>{RuleVariant(vector<Atom>{array + 2, "struct", array + 1, array + 3, array + 2, "{"}, vector<Atom>{array + 45, array + 1, array + 3, array + 2, ";"}, vector<Atom>{array + 2, "}"})});
new(array + 30)Rule(30, "_union", vector<RuleVariant>{RuleVariant(vector<Atom>{array + 2, "union", array + 1, array + 3, array + 2, "{"}, vector<Atom>{array + 45, array + 1, array + 3, array + 2, ";"}, vector<Atom>{array + 2, "}"})});
new(array + 31)Rule(31, "add_op", vector<RuleVariant>{RuleVariant(vector<Atom>{array + 2, "+"}, vector<Atom>{}, vector<Atom>{}), RuleVariant(vector<Atom>{array + 2, "-"}, vector<Atom>{}, vector<Atom>{})});
new(array + 32)Rule(32, "arguments_list", vector<RuleVariant>{RuleVariant(vector<Atom>{}, vector<Atom>{array + 45, array + 1, array + 3, array + 2, ","}, vector<Atom>{array + 45, array + 1, array + 3}), RuleVariant(vector<Atom>{}, vector<Atom>{array + 45, array + 1, array + 3, array + 2, ","}, vector<Atom>{})});
new(array + 33)Rule(33, "attribute_list", vector<RuleVariant>{RuleVariant(vector<Atom>{}, vector<Atom>{array + 2, "[", array + 2, array + 4, array + 2, ":", array + 2, array + 4, array + 2, "]"}, vector<Atom>{})});
new(array + 34)Rule(34, "call_arg_list", vector<RuleVariant>{RuleVariant(vector<Atom>{}, vector<Atom>{array + 39, array + 2, ","}, vector<Atom>{array + 39}), RuleVariant(vector<Atom>{}, vector<Atom>{array + 39, array + 2, ","}, vector<Atom>{})});
new(array + 35)Rule(35, "call_result_list", vector<RuleVariant>{RuleVariant(vector<Atom>{}, vector<Atom>{array + 42, array + 2, ","}, vector<Atom>{array + 42}), RuleVariant(vector<Atom>{}, vector<Atom>{array + 42, array + 2, ","}, vector<Atom>{})});
new(array + 36)Rule(36, "case_branch", vector<RuleVariant>{RuleVariant(vector<Atom>{array + 39, array + 38}, vector<Atom>{}, vector<Atom>{})});
new(array + 37)Rule(37, "cmp_op", vector<RuleVariant>{RuleVariant(vector<Atom>{array + 2, ">"}, vector<Atom>{}, vector<Atom>{}), RuleVariant(vector<Atom>{array + 2, ">="}, vector<Atom>{}, vector<Atom>{}), RuleVariant(vector<Atom>{array + 2, "<"}, vector<Atom>{}, vector<Atom>{}), RuleVariant(vector<Atom>{array + 2, "<="}, vector<Atom>{}, vector<Atom>{}), RuleVariant(vector<Atom>{array + 2, "="}, vector<Atom>{}, vector<Atom>{}), RuleVariant(vector<Atom>{array + 2, "<>"}, vector<Atom>{}, vector<Atom>{})});
new(array + 38)Rule(38, "code_block", vector<RuleVariant>{RuleVariant(vector<Atom>{array + 2, "{"}, vector<Atom>{array + 43}, vector<Atom>{array + 2, "}"})});
new(array + 39)Rule(39, "expression", vector<RuleVariant>{RuleVariant(vector<Atom>{array + 26}, vector<Atom>{}, vector<Atom>{})});
new(array + 40)Rule(40, "mul_op", vector<RuleVariant>{RuleVariant(vector<Atom>{array + 2, "*"}, vector<Atom>{}, vector<Atom>{}), RuleVariant(vector<Atom>{array + 2, "/"}, vector<Atom>{}, vector<Atom>{})});
new(array + 41)Rule(41, "new_operator", vector<RuleVariant>{RuleVariant(vector<Atom>{array + 2, "new", array + 33, array + 2, array + 3, array + 2, "(", array + 34, array + 2, ")"}, vector<Atom>{}, vector<Atom>{}), RuleVariant(vector<Atom>{array + 2, "new", array + 1, array + 3, array + 2, "(", array + 34, array + 2, ")"}, vector<Atom>{}, vector<Atom>{})});
new(array + 42)Rule(42, "result_list_identifer", vector<RuleVariant>{RuleVariant(vector<Atom>{array + 2, "*"}, vector<Atom>{}, vector<Atom>{}), RuleVariant(vector<Atom>{array + 2, array + 3}, vector<Atom>{}, vector<Atom>{})});
new(array + 43)Rule(43, "statement", vector<RuleVariant>{RuleVariant(vector<Atom>{array + 44, array + 2, ";"}, vector<Atom>{}, vector<Atom>{}), RuleVariant(vector<Atom>{array + 39, array + 2, ";"}, vector<Atom>{}, vector<Atom>{}), RuleVariant(vector<Atom>{array + 2, "while", array + 39, array + 38}, vector<Atom>{}, vector<Atom>{}), RuleVariant(vector<Atom>{array + 2, "match", array + 39, array + 2, "{"}, vector<Atom>{array + 36}, vector<Atom>{array + 2, "}"})});
new(array + 44)Rule(44, "var_declaration", vector<RuleVariant>{RuleVariant(vector<Atom>{array + 45, array + 1, array + 3}, vector<Atom>{array + 2, ",", array + 2, array + 3}, vector<Atom>{array + 2, ","}), RuleVariant(vector<Atom>{array + 45, array + 1, array + 3}, vector<Atom>{array + 2, ",", array + 2, array + 3}, vector<Atom>{})});
new(array + 45)Rule(45, "var_type", vector<RuleVariant>{RuleVariant(vector<Atom>{array + 2, array + 3}, vector<Atom>{array + 46}, vector<Atom>{})});
new(array + 46)Rule(46, "var_type_moditifer", vector<RuleVariant>{RuleVariant(vector<Atom>{array + 2, "[]"}, vector<Atom>{}, vector<Atom>{}), RuleVariant(vector<Atom>{array + 2, "|"}, vector<Atom>{}, vector<Atom>{}), RuleVariant(vector<Atom>{array + 2, "?"}, vector<Atom>{}, vector<Atom>{})});
new(array + 47)Rule(47, "worker", vector<RuleVariant>{RuleVariant(vector<Atom>{array + 33, array + 2, "(", array + 32, array + 2, ")", array + 2, array + 3, array + 2, "(", array + 32, array + 2, ")", array + 38}, vector<Atom>{}, vector<Atom>{})});
new(array + 48)Rule(48, "worker_decl", vector<RuleVariant>{RuleVariant(vector<Atom>{array + 33, array + 2, "(", array + 32, array + 2, ")", array + 2, array + 3, array + 2, "(", array + 32, array + 2, ")", array + 2, ";"}, vector<Atom>{}, vector<Atom>{})});
return array;
}