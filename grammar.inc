new(array + 1)Rule(1, "S", vector<RuleVariant>{RuleVariant(vector<Atom>{grammar_fn_spaces})});
new(array + 2)Rule(2, "Sn", vector<RuleVariant>{RuleVariant(vector<Atom>{grammar_fn_spaces_or_no})});
new(array + 3)Rule(3, "identifer", vector<RuleVariant>{RuleVariant(vector<Atom>{grammar_fn_identifer})});
new(array + 4)Rule(4, "identifer_or_number", vector<RuleVariant>{RuleVariant(vector<Atom>{grammar_fn_identifer_or_number})});
new(array + 5)Rule(5, "integer", vector<RuleVariant>{RuleVariant(vector<Atom>{grammar_fn_integer})});
new(array + 6)Rule(6, "float", vector<RuleVariant>{RuleVariant(vector<Atom>{grammar_fn_float})});
