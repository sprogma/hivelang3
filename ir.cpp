#include <stdio.h>
#include <string.h>
#include <map>
#include <string>
#include <vector>
#include <variant>
#include <algorithm>

using namespace std;

#include "logger.hpp"
#include "ast.hpp"
#include "ir.hpp"

pair<vector<Worker *>, bool> buildAst(const char *filename, char *source, vector<Node *>nodes)
{
    logError(filename, source, nodes[0]->start);
    return {{}, false};
}
