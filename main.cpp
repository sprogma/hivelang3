#include <vector>
#include <stdio.h>
using namespace std;


#include "ast.hpp"
#include "optimization/optimizer.hpp"
#include "ir.hpp"


int main(int argc, char **argv)
{
    map<string, string> configs;

    for (int i = 1; i < argc; ++i) 
    {
        string s = argv[i];

        size_t eq = s.find('=');
        if (eq == string::npos) continue;

        string key = s.substr(0, eq);
        string val = s.substr(eq + 1);

        if (key.size() >= 4 && key[0] == '-') 
        {
            size_t pos = 0;
            while (pos < key.size() && key[pos] == '-')  { ++pos; }
            string param = key.substr(pos);
            if (!param.empty()) 
            {
                configs[param] = val;
            }
        }
    }

    
    const char *filename = "example.hive";
    FILE *f = fopen(filename, "r");
    char *code = (char *)malloc(1024 * 1024);
    code[fread(code, 1, 1024 * 1024, f)] = 0;
    fclose(f);

    grammar = generateGrammar();

    Rule *rule = grammarGetRule("Global");
    if (rule == NULL)
    {
        printf("Error: no rule named \"Global\"");
        return 1;
    }

    auto [nodes, error] = parse(filename, rule, code);

    if (error)
    {
        printf("Error: parsing failed\n");
        return 1;
    }

    printf("Parsed\n");
    
    printf("Convering...\n");
    /* convert to intermediate language */

    auto [Code, error2] = buildAst(filename, code, nodes, configs);

    if (error2)
    {
        printf("Error: building ast failed\n");
        return 2;
    }
    
    printf("Ast builded\n");

    for (auto &[fn, key] : Code->workers)
    {
        dumpIR(fn);
    }

    printf("Optimization layers?\n");

    Optimizer opt;
    opt.AddLayer(newInlineLayer(10.0));

    auto newCode = opt.Apply(Code);

    printf("After optimization have code:\n");

    for (auto &[fn, key] : newCode->workers)
    {
        dumpIR(fn);
    }

    free(code);
    return 0;
}
