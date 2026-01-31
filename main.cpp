#include <vector>
#include <stdio.h>
using namespace std;


#include "ast.hpp"
#include "optimization/optimizer.hpp"
#include "codegen/codegen.hpp"
#include "ir.hpp"


int main(int argc, char **argv)
{
    fclose(fopen("D:/mipt/lang3/started", "w"));
    
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

    // TODO: better comments support
    // replace all comments with spaces
    {
        char *s = code;
        int str = 0;
        while (*s)
        {
            if (str)
            {
                if (s[0] == '\\')
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

    if (configs.contains("syntax-only"))
    {
        return 0;
    }

    for (auto &[fn, key] : Code->workers)
    {
        dumpIR(fn);
    }

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

    /* generate workers for each provider */
    FILE *o = fopen("res.bin", "wb");

    BYTE *header = (BYTE *)malloc(1024 * 1024);
    BYTE *header_start = header;
    BYTE *body = (BYTE *)malloc(1024 * 1024);
    BYTE *body_start = body;

    *(uint64_t *)header = 3; // version 0.3
    header += 8;
    /* generate header */
    *(uint64_t *)header = 0xBEBEBEBEBEBEBEBE;
    header += 8;
    /* generate prefix */
    *header++ = 'H'; *header++ = 'I'; *header++ = 'V'; *header++ = 'E';
    
    for (auto &name : newCode->used_providers)
    {
        if (name == "x64")
        {
            CodeAssembler *assembler = new_x64_Assembler();
            tie(header, body) = assembler->Build(newCode, header, body, body - body_start);
        }
    }

    /* fill header size */
    *(uint64_t *)(header_start + 12) = header - header_start;


    int64_t totalBytes = (header - header_start) + (body - body_start);
    fwrite(header_start, 1, header - header_start, o);
    fwrite(body, 1, body - body_start, o);

    fclose(o);

    printf("%lld bytes written\n", totalBytes);
    printf("res.exe file generated\n");

    free(code);
    return 0;
}
