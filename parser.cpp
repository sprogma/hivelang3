#include <stdio.h>
#include <string.h>
#include <map>
#include <string>
#include <vector>
#include <variant>
#include <algorithm>

using namespace std;

#include "ast.hpp"

template<class... Fs> struct overload : Fs... { using Fs::operator()...; };
template<class... Fs> overload(Fs...) -> overload<Fs...>;

// rule id + position -> result + position
map<pair<int64_t, int64_t>, pair<Node *, int64_t>> cache;

struct ParsingResult
{
    Node *res;
    int64_t position;
};


pair<Node *, int64_t> parseRule(Rule *rule, char *content, int64_t position);

pair<Node *, int64_t> parseAtom(Atom &atom, char *content, int64_t position)
{
    return std::visit(overload{
        [&](Rule *const &atom_rule) -> pair<Node*,int64_t> {
            auto [res, pos] = parseRule(atom_rule, content, position);
            if (res == NULL) { return {NULL, pos}; }
            return {res, pos};
        },
        [&](const char *const &literal) -> pair<Node*,int64_t> { 
            int64_t end = position + strlen(literal);
            if (strncmp(content + position, literal, end - position) == 0)
                return {new Node(NULL, 0, position, end, 0, NULL), end};
            return {NULL, position};
        },
        [&](pair<Node*, int64_t>(*function)(char *, int64_t)){ return function(content, position); }
    }, atom);
}


pair<Node *, int64_t> parseVariant(Rule *rule, int64_t variantId, RuleVariant &var, char *content, int64_t position)
{
    vector<Node *> childs;
    /* apply prefix */
    for (Atom x : var.prefix)
    {
        auto [res, pos] = parseAtom(x, content, position);
        if (!res) { return {NULL, pos}; }
        childs.push_back(res);
        position = pos;
    }
    /* apply period */
    while (1)
    {
        vector<Node *> period;
        for (Atom x : var.period)
        {
            auto [res, pos] = parseAtom(x, content, position);
            if (!res) { break; }
            period.push_back(res);
            position = pos;
        }
        if (period.size() == var.period.size())
        {
            childs.insert(childs.end(), period.begin(), period.end());
        }
        else
        {
            break;
        }
    }
    /* apply suffix */
    for (Atom x : var.suffix)
    {
        auto [res, pos] = parseAtom(x, content, position);
        if (!res) { return {NULL, pos}; }
        childs.push_back(res);
        position = pos;
    }
    Node **array = (Node **)malloc(sizeof(*array) * childs.size());
    memcpy(array, childs.data(), sizeof(*array) * childs.size());
    return {new Node(rule, variantId, childs.front()->start, childs.back()->end, childs.size(), array), position};
}


pair<Node *, int64_t> parseRule(Rule *rule, char *content, int64_t position)
{
    map<pair<int64_t, int64_t>, pair<Node *, int64_t>>::iterator it;
    if ((it = cache.find({rule->id, position})) == cache.end())
    {
        /* if not cached - try all variants */
        int64_t max_parsed_position = 0;
        int64_t varId = 0;
        for (auto var : rule->variants)
        {
            auto [res, pos] = parseVariant(rule, varId, var, content, position);
            max_parsed_position = max(pos, max_parsed_position);
            if (res)
            {
                it = cache.insert({{rule->id, position}, {res, pos}}).first;
                break;
            }
            varId++;
        }
        if (it == cache.end())
        {
            it = cache.insert({{rule->id, position}, {NULL, max_parsed_position}}).first;
        }
    }
    return it->second;
}

pair<vector<Node *>, bool>parse(Rule *baseRule, char *content)
{
    cache.clear();
    vector<Node *> result;
    int64_t position = 0;
    bool prev_was_error = false, any_errors = false;
    while (content[position])
    {
        auto [res, pos] = parseRule(baseRule, content, position);
        if (res == NULL)
        {
            if (!prev_was_error)
            {
                char *prev_newline = content + pos;
                while (prev_newline > content && *prev_newline != '\n') { prev_newline--; }
                char *next_newline = strchr(content + pos, '\n') ?: content + strlen(content);
                int64_t line = count(content, content + pos, '\n');
                printf("Error: can't parse near file:%lld:%lld> %.*s\n", line, 
                                                                        (content + pos) - prev_newline, 
                                                                        (int)(next_newline - prev_newline),
                                                                        prev_newline);
            }
            position = pos + 1;
            prev_was_error = true;
            any_errors = true;
        }
        else
        {
            prev_was_error = false;
            result.push_back(res);
        }
    }
    return {result, any_errors};
}
