#include "http_request_parser.h"

#include "../log/log.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <stdexcept>

namespace
{
    std::string lowercase_copy(const std::string &value)
    {
        std::string lowered = value;
        std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                       [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        return lowered;
    }

    std::string trim_copy(const std::string &value)
    {
        size_t first = value.find_first_not_of(" \t\r\n");
        if (first == std::string::npos)
            return "";

        size_t last = value.find_last_not_of(" \t\r\n");
        return value.substr(first, last - first + 1);
    }

    bool starts_with(const std::string &value, const std::string &prefix)
    {
        return value.compare(0, prefix.size(), prefix) == 0;
    }

    enum class ContentLengthResult
    {
        OK,
        INVALID,
        TOO_LARGE
    };

    ContentLengthResult parse_content_length(const std::string &value, size_t &length)
    {
        if (value.empty())
            return ContentLengthResult::INVALID;

        size_t parsed = 0;
        for (char ch : value)
        {
            if (!std::isdigit(static_cast<unsigned char>(ch)))
                return ContentLengthResult::INVALID;

            size_t digit = static_cast<size_t>(ch - '0');
            if (parsed > (HTTP_MAX_BODY_BYTES - digit) / 10)
                return ContentLengthResult::TOO_LARGE;

            parsed = parsed * 10 + digit;
            if (parsed > HTTP_MAX_BODY_BYTES)
                return ContentLengthResult::TOO_LARGE;
        }

        length = parsed;
        return ContentLengthResult::OK;
    }

    METHOD method_from_text(const std::string &method)
    {
        if (method == "GET")
            return GET;
        if (method == "POST")
            return POST;
        if (method == "HEAD")
            return HEAD;
        if (method == "PUT")
            return PUT;
        if (method == "DELETE")
            return DELETE;
        if (method == "TRACE")
            return TRACE;
        if (method == "OPTIONS")
            return OPTIONS;
        if (method == "CONNECT")
            return CONNECT;
        if (method == "PATCH")
            return PATCH;
        return UNKNOWN;
    }

    bool method_is_supported(METHOD method)
    {
        // This cleanup keeps the current project scope: route GET and POST first.
        return method == GET || method == POST;
    }

    bool target_has_traversal(const std::string &target)
    {
        // Reject raw ".." early so /../README.md never reaches the static file layer.
        return target.find("..") != std::string::npos;
    }

    std::string path_from_absolute_uri(const std::string &target)
    {
        if (!starts_with(target, "http://") && !starts_with(target, "https://"))
            return target;

        size_t scheme_end = target.find("://");
        size_t path_start = target.find('/', scheme_end + 3);
        if (path_start == std::string::npos)
            return "/";

        return target.substr(path_start);
    }
}

HttpRequestParser::HttpRequestParser()
{
    reset();
}

void HttpRequestParser::reset()
{
    header_bytes_ = 0;
    body_bytes_ = 0;
}

HttpRequestParser::Result HttpRequestParser::parse(const std::string &buffer, HttpRequest &request)
{
    size_t header_end = buffer.find("\r\n\r\n");
    size_t delimiter_size = 4;

    if (header_end == std::string::npos)
    {
        header_end = buffer.find("\n\n");
        delimiter_size = 2;
    }

    if (header_end == std::string::npos)
    {
        // Until the blank line arrives, every byte belongs to the header section.
        return buffer.size() > HTTP_MAX_HEADER_BYTES ? Result::PAYLOAD_TOO_LARGE : Result::INCOMPLETE;
    }

    if (header_end > HTTP_MAX_HEADER_BYTES)
        return Result::PAYLOAD_TOO_LARGE;

    HttpRequest parsed;
    std::string header_block = buffer.substr(0, header_end);
    std::istringstream stream(header_block);
    std::string line;

    if (!std::getline(stream, line))
        return Result::BAD_REQUEST;

    line = trim_copy(line);
    std::istringstream request_line(line);
    std::string method;
    std::string target;
    std::string version;
    std::string extra;

    request_line >> method >> target >> version >> extra;
    if (method.empty() || target.empty() || version.empty() || !extra.empty())
        return Result::BAD_REQUEST;

    METHOD parsed_method = method_from_text(method);
    if (parsed_method == UNKNOWN)
        return Result::BAD_REQUEST;

    if (!method_is_supported(parsed_method))
        return Result::METHOD_NOT_ALLOWED;

    if (version != "HTTP/1.1" && version != "HTTP/1.0")
        return Result::BAD_REQUEST;

    target = path_from_absolute_uri(target);
    if (target.empty() || target[0] != '/')
        return Result::BAD_REQUEST;

    if (target_has_traversal(target))
        return Result::BAD_REQUEST;

    parsed.method_type = parsed_method;
    parsed.method = method;
    parsed.target = target;
    parsed.version = version;

    size_t query_start = target.find('?');
    parsed.path = query_start == std::string::npos ? target : target.substr(0, query_start);
    parsed.query = query_start == std::string::npos ? "" : target.substr(query_start + 1);
    if (parsed.path == "/")
        parsed.path = "/judge.html";

    while (std::getline(stream, line))
    {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        size_t colon = line.find(':');
        if (colon == std::string::npos)
            return Result::BAD_REQUEST;

        std::string name = lowercase_copy(trim_copy(line.substr(0, colon)));
        std::string value = trim_copy(line.substr(colon + 1));

        if (name.empty())
            return Result::BAD_REQUEST;

        parsed.headers[name] = value;
    }

    parsed.host = parsed.get_header("host");
    parsed.keep_alive = parsed.version == "HTTP/1.1";

    std::string connection = lowercase_copy(parsed.get_header("connection"));
    if (connection == "close")
        parsed.keep_alive = false;
    else if (connection == "keep-alive")
        parsed.keep_alive = true;

    std::string content_length_header = parsed.get_header("content-length");
    if (!content_length_header.empty())
    {
        ContentLengthResult length_result = parse_content_length(content_length_header, body_bytes_);
        if (length_result == ContentLengthResult::INVALID)
            return Result::BAD_REQUEST;
        if (length_result == ContentLengthResult::TOO_LARGE)
            return Result::PAYLOAD_TOO_LARGE;
    }
    else
    {
        body_bytes_ = 0;
    }

    header_bytes_ = header_end + delimiter_size;
    if (buffer.size() < header_bytes_ + body_bytes_)
        return Result::INCOMPLETE;

    parsed.content_length = body_bytes_;
    parsed.body = buffer.substr(header_bytes_, body_bytes_);

    std::string content_type = lowercase_copy(parsed.get_header("content-type"));
    if (!parsed.body.empty() && content_type.find("application/json") != std::string::npos)
    {
        try
        {
            // JSON parsing is deliberately tied to Content-Type so form posts stay as raw body text.
            parsed.json_body = JSON::parse(parsed.body);
        }
        catch (const std::exception &ex)
        {
            LOG_ERROR("Failed to parse JSON body: %s", ex.what());
            return Result::BAD_REQUEST;
        }
    }

    request = parsed;
    return Result::COMPLETE;
}
