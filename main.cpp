#include <vector>
#include <stdio.h>
using namespace std;


#include "ast.hpp"


int main()
{
    FILE *f = fopen("example.hive", "r");
    char *code = (char *)malloc(1024 * 1024);
    code[fread(code, 1, 1024 * 1024, f)] = 0;
    fclose(f);

    grammar = generateGrammar();

    auto [nodes, error] = parse(grammar + 20, code);

    if (error)
    {
        printf("Error: parsing failed\n");
        return 1;
    }

    printf("Parsed\n");

    free(code);
    return 0;
}
