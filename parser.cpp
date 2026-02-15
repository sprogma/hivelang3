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
#include "utils.hpp"



pair<Node *, int64_t> parseAtom(Atom &atom, char *content, int64_t position);
pair<Node *, int64_t> parseRule(Rule *rule, char *content, int64_t position);
pair<Node *, int64_t> parseVariant(Rule *rule, int64_t variantId, RuleVariant &var, char *content, int64_t position);



// rule id + position -> result + position
map<pair<int64_t, int64_t>, pair<Node *, int64_t>> cache;



pair<Node *, int64_t> parseAtom(Atom &atom, char *content, int64_t position)
{
    return visit(overload{
        [&](Rule *const &atom_rule) -> pair<Node*,int64_t> {
            auto [res, pos] = parseRule(atom_rule, content, position);
            if (res == NULL) { return {NULL, pos}; }
            return {res, pos};
        },
        [&](const char *const &literal) -> pair<Node*,int64_t> { 
            int64_t end = position + strlen(literal);
            if (strncmp(content + position, literal, end - position) == 0)
                return {new Node(NULL, 0, position, end, {}), end};
            return {NULL, position};
        },
        [&](pair<Node*, int64_t>(*function)(char *, int64_t)){ return function(content, position); }
    }, atom);
}



tuple<int64_t, int64_t, bool> variantApplySeq(const vector<Atom> &seq, vector<Node *> &childs, char *content, int64_t position)
{
    int64_t max_parsed = position;
    for (Atom x : seq)
    {
        auto [res, pos] = parseAtom(x, content, position);
        max_parsed = max(max_parsed, pos);
        if (!res) { return {max_parsed, position, false}; }
        childs.push_back(res);
        position = pos;
    }
    return {max_parsed, position, true};
}



pair<Node *, int64_t> parseVariant(Rule *rule, int64_t variantId, RuleVariant &var, char *content, int64_t position)
{
    int64_t max_parsed = position;
    bool ok;
    vector<Node *> childs;

    /* apply prefix */
    tie(max_parsed, position, ok) = variantApplySeq(var.prefix, childs, content, position);
    if (!ok)
    {
        return {NULL, max_parsed};
    }

    /* apply period */
    while (var.period.size() > 0)
    {
        vector<Node *> period;
        auto [next_max, next_position, period_ok] = variantApplySeq(var.period, period, content, position);
        max_parsed = max(max_parsed, next_max);
        if (period.size() != var.period.size() || !period_ok)
        {
            break;
        }
        childs.insert(childs.end(), period.begin(), period.end());
        position = next_position;
    }
    
    /* apply suffix */
    int64_t suffix_max;
    tie(suffix_max, position, ok) = variantApplySeq(var.suffix, childs, content, position);
    if (!ok)
    {
        return {NULL, max_parsed};
    }
    max_parsed = max(max_parsed, suffix_max);

    
    if (childs.size() == 0)
    {
        return {new Node(rule, variantId, position, position, {}), position};
    }
    return {new Node(rule, variantId, childs.front()->start, childs.back()->end, childs), position};
}


pair<Node *, int64_t> parseRule(Rule *rule, char *content, int64_t position)
{
    // printf("apply %s to %lld\n", rule->name, position);
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
    // printf("apply [%s] res: %p %lld\n", rule->name, it->second.first, it->second.second);
    return it->second;
}


pair<vector<Node *>, bool>parse(const char *filename, Rule *baseRule, char *content)
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
                logError(filename, content, pos, "Can't parse this construction");
            }
            position = pos + 1;
            prev_was_error = true;
            any_errors = true;
        }
        else
        {
            if (pos == position)
            {
                logError(filename, content, position, "parsing global rule don't moved position - this is infinite loop [empty pattern matched] - skipping next simbol");
                position++;
            }
            else
            {
                position = pos;
            }
            prev_was_error = false;
            result.push_back(res);
        }
    }
    return {result, any_errors};
}

void dumpAst(Node *x, int t)
{
    for (int i = 0; i < t; ++i) { printf("   "); }
    if (x->rule) { printf("Node [%s]\n", x->rule->name); }
    else { printf("Literal\n"); }
    for (auto Z : x->childs) { dumpAst(Z, t + 1); }
}

