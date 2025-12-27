#include <vector>
#include <stdio.h>
using namespace std;


#include "ast.hpp"
#include "ir.hpp"


int main()
{
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

    auto [Code, error2] = buildAst(filename, code, nodes);

    if (error2)
    {
        printf("Error: building ast failed\n");
        return 2;
    }
    
    printf("Ast builded\n");

    for (auto &fn : Code->workers)
    {
        printf("Worker %s\n", fn->name.c_str());
        if (fn->content)
        {
            printf("code:\n");
            for (auto &x : fn->content->code)
            {
                printf("%d [%p]\n", x.type, x.data);
            }
        }
    }

    free(code);
    return 0;
}
