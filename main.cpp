#include <vector>
#include <stdio.h>
using namespace std;


#include "ast.hpp"
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

    for (auto &fn : Code->workers)
    {
        printf("Worker %s\n", fn->name.c_str());
        if (fn->content)
        {
            printf("code:\n");
            for (auto &x : fn->content->code)
            {
                switch (x.type)
                {
                
                    case OP_LOAD_INPUT: printf("OP_LOAD_INPUT "); break;
                    case OP_LOAD_OUTPUT: printf("OP_LOAD_OUTPUT "); break;
                    
                    case OP_FREE_TEMP: printf("OP_FREE_TEMP "); break;
                    
                    case OP_CALL: printf("OP_CALL "); break;
                    case OP_CAST: printf("OP_CAST "); break;
    
                    case OP_NEW_INT: printf("OP_NEW_INT "); break;
                    case OP_NEW_FLOAT: printf("OP_NEW_FLOAT "); break;
                    case OP_NEW_ARRAY: printf("OP_NEW_ARRAY "); break;
                    case OP_NEW_PIPE: printf("OP_NEW_PIPE "); break;
                    case OP_NEW_PROMISE: printf("OP_NEW_PROMISE "); break;
                    case OP_NEW_CLASS: printf("OP_NEW_CLASS "); break;
                    
                    case OP_PUSH_VAR: printf("OP_PUSH_VAR "); break;
                    case OP_PUSH_ARRAY: printf("OP_PUSH_ARRAY "); break;
                    case OP_PUSH_PIPE: printf("OP_PUSH_PIPE "); break;
                    case OP_PUSH_PROMISE: printf("OP_PUSH_PROMISE "); break;
                    case OP_PUSH_CLASS: printf("OP_PUSH_CLASS "); break;
                    case OP_QUERY_VAR: printf("OP_QUERY_VAR "); break;
                    case OP_QUERY_ARRAY: printf("OP_QUERY_ARRAY "); break;
                    case OP_QUERY_INDEX: printf("OP_QUERY_INDEX "); break;
                    case OP_QUERY_PIPE: printf("OP_QUERY_PIPE "); break;
                    case OP_QUERY_PROMISE: printf("OP_QUERY_PROMISE "); break;
                    case OP_QUERY_CLASS: printf("OP_QUERY_CLASS "); break;
                    
                    case OP_BOR: printf("OP_BOR "); break;
                    case OP_BAND: printf("OP_BAND "); break;
                    case OP_BXOR: printf("OP_BXOR "); break;
                    case OP_SHL: printf("OP_SHL "); break;
                    case OP_SHR: printf("OP_SHR "); break;
                    case OP_BNOT: printf("OP_BNOT "); break;
                    
                    case OP_JMP: printf("OP_JMP "); break;
                    case OP_JZ: printf("OP_JZ "); break;
                    case OP_JNZ: printf("OP_JNZ "); break;
                    
                    case OP_ADD: printf("OP_ADD "); break;
                    case OP_SUB: printf("OP_SUB "); break;
                    case OP_MUL: printf("OP_MUL "); break;
                    case OP_DIV: printf("OP_DIV "); break;
                    case OP_MOD: printf("OP_MOD "); break;
                    case OP_EQ: printf("OP_EQ "); break;
                    case OP_LT: printf("OP_LT "); break;
                    case OP_LE: printf("OP_LE "); break;
                    case OP_GT: printf("OP_GT "); break;
                    case OP_GE: printf("OP_GE "); break;
                }
                printf("[ ");
                for (int64_t t : x.data)
                {
                    printf("%lld ", t);
                }
                printf("]");
                if (x.attributes.size() > 0)
                {
                    printf(" { ");
                    for (auto [k, v] : x.attributes)
                    {
                        printf("%s=%s ", k.c_str(), v.c_str());
                    }
                    printf("}");
                }
                printf("\n");
            }
        }
    }

    free(code);
    return 0;
}
