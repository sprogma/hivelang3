#include <vector>
#include <stdio.h>
using namespace std;


#include "ast.hpp"
#include "optimization/optimizer.hpp"
#include "codegen/codegen.hpp"
#include "ir.hpp"


map<string, string> ParseComandlineArgs(int argc, char **argv)
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
    return configs;
}


int main(int argc, char **argv)
{    
    map<string, string> configs = ParseComandlineArgs(argc, argv);
    
    const char *filename = "D:\\mipt\\lang3\\example.hive";
    if (configs.contains("input-file"))
    {
        filename = configs["input-file"].c_str();
    }

    
    FILE *f = fopen(filename, "r");
    if (f == NULL)
    {
        printf("Error: input file %s not found\n", filename);
        return 1;
    }
    char *code = (char *)malloc(1024 * 1024);
    code[fread(code, 1, 1024 * 1024, f)] = 0;
    fclose(f);
    

    // TODO: better comments support
    // replace all comments with spaces
    {
        char *s = code;
        int str = 0;
        while (*s)
        {
            if (str)
            {
                if (s[0] == '\\' && s[1] != 0)
                {
                    s += 2;
                }
                else if (s[0] == '\"')
                {
                    str = 0;
                    s += 1;
                }
                else
                {
                    s += 1;
                }
            }
            else
            {
                if (s[0] == '#')
                {
                    while (*s && *s != '\n')
                    {
                        *s++ = ' ';
                    }
                }
                else if (s[0] == '\"')
                {
                    str = 1;
                    s += 1;
                }
                else
                {
                    s++;
                }
            }
        }
    }


    // parse file
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

    auto [Code, error2] = buildAst(filename, code, nodes, configs, "x64");
    if (error2)
    {
        printf("Error: building ast failed\n");
        return 2;
    }

    
    printf("Ast builded\n");


    if (configs.contains("syntax-only"))
    {
        return 0;
    }
    
    for (auto &[fn, key] : Code->workers)
    {
        dumpIR(fn);
    }

    /* start optimization */
    printf("Optimization layers?\n");

    Optimizer opt;
    opt.AddLayer(newInlineLayer(10.0));
    opt.AddLayer(newCompressPromiseLayer());
    opt.AddLayer(newStripUnusedInstructionsLayer());
    opt.AddLayer(newStripUnusedFunctionsLayer());

    auto newCode = opt.Apply(Code);

    printf("After optimization have code:\n");

    for (auto &[fn, key] : newCode->workers)
    {
        dumpIR(fn);
    }

    if (ExportCode(newCode))
    {
        printf("Error happen while exporting code\n");
    }

    // TODO: free(newCode)
    free(code);

    return 0;
}
