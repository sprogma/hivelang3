#include <iostream>
#include <string>
#include <string_view>

using namespace std;

#include "../headers/logger.hpp"


string xml_encode(string_view data) 
{
    string result;
    result.reserve(data.size() + 32);

    const string_view targets = "&<>\'\"";
    size_t start = 0;
    size_t pos;

    while ((pos = data.find_first_of(targets, start)) != string_view::npos) 
    {
        result.append(data.substr(start, pos - start));

        switch (data[pos]) {
            case '&':  result.append("&amp;");   break;
            case '<':  result.append("&lt;");    break;
            case '>':  result.append("&gt;");    break;
            case '\'': result.append("&apos;");  break;
            case '\"': result.append("&quot;");  break;
        }
        start = pos + 1;
    }

    result.append(data.substr(start));
    return result;
}
