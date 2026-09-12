#include "http/http_request_parser.h"
#include "http/http_response.h"

#include <cassert>
#include <cstring>
#include <iostream>
#include <string>
#include <unistd.h>

namespace
{
    std::string response_bytes(const HttpResponse &response)
    {
        std::string bytes;
        size_t offset = 0;

        while (offset < response.total_bytes())
        {
            struct iovec iov[2];
            int count = response.build_iovecs(offset, iov);
            assert(count > 0);

            for (int i = 0; i < count; ++i)
            {
                // iov_base points into response-owned strings or mmap() memory, so copy before changing offset.
                bytes.append(static_cast<const char *>(iov[i].iov_base), iov[i].iov_len);
                offset += iov[i].iov_len;
            }
        }

        return bytes;
    }

    std::string project_root()
    {
        char cwd[1024];
        assert(getcwd(cwd, sizeof(cwd)) != nullptr);
        return std::string(cwd);
    }
}

int main()
{
    {
        HttpRequestParser parser;
        HttpRequest request;
        std::string raw = "GET /about HTTP/1.1\r\nHost: example.test\r\n\r\n";

        assert(parser.parse(raw, request) == HttpRequestParser::Result::COMPLETE);
        assert(request.method == "GET");
        assert(request.path == "/about");
        assert(request.query.empty());
        assert(request.keep_alive == true);
        assert(request.get_header("HOST") == "example.test");
    }

    {
        HttpRequestParser parser;
        HttpRequest request;
        std::string raw = "POST /login HTTP/1.1\r\n"
                          "Host: example.test\r\n"
                          "Content-Type: application/x-www-form-urlencoded\r\n"
                          "Content-Length: 11\r\n"
                          "Connection: close\r\n"
                          "\r\n"
                          "hello=world";

        assert(parser.parse(raw, request) == HttpRequestParser::Result::COMPLETE);
        assert(request.method == "POST");
        assert(request.path == "/login");
        assert(request.body == "hello=world");
        assert(request.content_length == 11);
        assert(request.keep_alive == false);
    }

    {
        HttpRequestParser parser;
        HttpRequest request;
        std::string partial = "POST /login HTTP/1.1\r\nContent-Length: 11\r\n\r\nhello";
        std::string complete = partial + "=world";

        assert(parser.parse(partial, request) == HttpRequestParser::Result::INCOMPLETE);
        assert(parser.parse(complete, request) == HttpRequestParser::Result::COMPLETE);
        assert(request.body == "hello=world");
    }

    {
        HttpRequestParser parser;
        HttpRequest request;

        assert(parser.parse("GET /missing-version\r\nHost: x\r\n\r\n", request) == HttpRequestParser::Result::BAD_REQUEST);
        assert(parser.parse("PUT /about HTTP/1.1\r\nHost: x\r\n\r\n", request) == HttpRequestParser::Result::METHOD_NOT_ALLOWED);
        assert(parser.parse("GET /../README.md HTTP/1.1\r\nHost: x\r\n\r\n", request) == HttpRequestParser::Result::BAD_REQUEST);
        assert(parser.parse("POST /login HTTP/1.1\r\nContent-Length: abc\r\n\r\n", request) == HttpRequestParser::Result::BAD_REQUEST);
    }

    {
        HttpResponse response;
        response.set_keep_alive(true);
        assert(response.send(200, "hello"));

        std::string bytes = response_bytes(response);
        assert(bytes.find("HTTP/1.1 200 OK\r\n") == 0);
        assert(bytes.find("Content-Length: 5\r\n") != std::string::npos);
        assert(bytes.find("Connection: keep-alive\r\n") != std::string::npos);
        assert(bytes.rfind("hello") == bytes.size() - 5);
    }

    {
        HttpResponse response;
        response.set_doc_root(project_root() + "/root");
        assert(response.render(200, "/judge.html"));

        std::string bytes = response_bytes(response);
        assert(bytes.find("HTTP/1.1 200 OK\r\n") == 0);
        assert(bytes.find("Content-Type: text/html\r\n") != std::string::npos);
        assert(bytes.find("<html") != std::string::npos || bytes.find("<!DOCTYPE") != std::string::npos || bytes.find("<!doctype") != std::string::npos);
        response.unmap();
    }

    {
        HttpResponse response;
        response.set_doc_root(project_root() + "/root");
        assert(!response.render(200, "/missing-file.html"));
        assert(!response.render(200, "/../README.md"));
    }

    std::cout << "http parser/response tests passed" << std::endl;
    return 0;
}
