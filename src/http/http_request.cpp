#include "http_request.h"

#include <algorithm>
#include <cctype>
#include <cstring>

namespace
{
    std::string lowercase_copy(const std::string &value)
    {
        std::string lowered = value;
        std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                       [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        return lowered;
    }
}

HttpRequest::HttpRequest()
{
    reset();
}

void HttpRequest::reset()
{
    method_type = GET;
    method.clear();
    target.clear();
    path.clear();
    query.clear();
    version.clear();
    host.clear();
    headers.clear();
    body.clear();
    content_length = 0;
    keep_alive = false;
    std::memset(&client_address, 0, sizeof(client_address));
    json_body = JSONNode();
}

std::string HttpRequest::get_header(const std::string &name) const
{
    auto it = headers.find(lowercase_copy(name));
    if (it == headers.end())
        return "";

    return it->second;
}

bool HttpRequest::has_header(const std::string &name) const
{
    return headers.find(lowercase_copy(name)) != headers.end();
}
