#include <vector>
#include <errno.h>
#include <string.h>
#include <ctype.h>

using namespace std;

#include "ast.hpp"

pair<Node *, int64_t>grammar_fn_spaces(char *content, int64_t position)
{
    if (!isspace(content[position]))
    {
        return {NULL, position};
    }
    int64_t start = position;
    while (isspace(content[position])) { position++; }
    return {new Node(grammar + 1, 0, start, position, 0, NULL), position};
}


pair<Node *, int64_t>grammar_fn_spaces_or_no(char *content, int64_t position)
{
    int64_t start = position;
    while (isalpha(content[position])) { position++; }
    return {new Node(grammar + 2, 0, start, position, 0, NULL), position};
}


pair<Node *, int64_t>grammar_fn_identifer(char *content, int64_t position)
{
    if (!isalpha(content[position]) && content[position] != '_')
    {
        return {NULL, position};
    }
    position++;
    int64_t start = position;
    while (isalpha(content[position]) || isdigit(content[position]) || content[position] == '_') { position++; }
    return {new Node(grammar + 3, 0, start, position, 0, NULL), position};
}


pair<Node *, int64_t>grammar_fn_integer(char *content, int64_t position)
{
    errno = 0;
    char* end = NULL;
    int64_t val = strtoll(content + position, &end, 0);
    (void)val;
    if (end == content + position || errno == ERANGE) 
    {
        return {NULL, position};
    }
    int64_t endvalue = end - content;
    return {new Node(grammar + 4, 0, position, endvalue, 0, NULL), endvalue};
}



pair<Node *, int64_t>grammar_fn_float(char *content, int64_t position)
{
    errno = 0;
    char* end = NULL;
    double val = strtod(content + position, &end);
    (void)val;
    if (end == content + position || errno == ERANGE) 
    {
        return {NULL, position};
    }
    int64_t endvalue = end - content;
    return {new Node(grammar + 5, 0, position, endvalue, 0, NULL), endvalue};
}
