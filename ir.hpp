#ifndef IR_HPP
#define IR_HPP

#include <vector>

using namespace std;

#include "ast.hpp"

struct Worker
{
    vector<int64_t> code;
};

pair<vector<Worker *>, bool> buildAst(const char *filename, char *code, vector<Node *>nodes);

#endif
