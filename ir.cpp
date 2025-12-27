#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <map>
#include <string>
#include <vector>
#include <variant>
#include <algorithm>

using namespace std;

#include "logger.hpp"
#include "ast.hpp"
#include "ir.hpp"

#define is(node, str) (strcmp(node->rule->name, str) == 0)

#define assert_type(node, str) assert(is(node, str))
#define switch_type(x) switch (x->rule ? x->rule->id : 0) 
#define switch_var(x) switch (x->variant) 

enum
{
    RECORD,
    UNION,
    CLASS,
};

void processStructure()
{
    
}


pair<vector<Worker *>, bool> buildAst(const char *filename, char *source, vector<Node *>nodes)
{
    ctx = new BuildContext()
    for (auto node : nodes)
    {
        assert_type(node, "Global");
        assert(node->childs_len == 1);

        if (is(node->childs[0], "S"))
        {
            /* skip empty node */
        }
        else if (is(node->childs[0], "_record"))
        {
            process_structure(ctx, RECORD, node->childs[0]);
        }
        else if (is(node->childs[0], "_union"))
        {
            
        }
        else if (is(node->childs[0], "_class"))
        {
            
        }
        else if (is(node->childs[0], "worker"))
        {
            
        }
        else if (is(node->childs[0], "worker_decl"))
        {
        }
        else
        {
            printf("Unknown global node type: <%s>\n", node->childs[0]->rule->name);
            logError(filename, source, node->childs[0]->start);
        }
    }
    return {{}, false};
}
