#ifndef HTTP_REQUEST_PARSER_H
#define HTTP_REQUEST_PARSER_H

#include "http_request.h"

#include <cstddef>
#include <string>

/**
 * @class HttpRequestParser
 * @brief Converts raw socket bytes into one HttpRequest.
 *
 * No HTTP pipelining is handled here: after one full request is parsed, the
 * connection resets this parser before reading the next keep-alive request.
 */
class HttpRequestParser
{
public:
    enum class Result
    {
        INCOMPLETE,         ///< Need more bytes before the request is complete.
        COMPLETE,           ///< A full, valid request has been parsed.
        BAD_REQUEST,        ///< Bytes are not valid HTTP syntax for this server.
        METHOD_NOT_ALLOWED, ///< Method is valid HTTP but not supported in this pass.
        PAYLOAD_TOO_LARGE   ///< Headers/body exceeded the configured limits.
    };

    HttpRequestParser();

    /**
     * @brief Forget the previous request parsing state.
     */
    void reset();

    /**
     * @brief Parse the bytes accumulated so far.
     * @param buffer Raw bytes read from the socket.
     * @param request Output object filled only when parsing succeeds.
     */
    Result parse(const std::string &buffer, HttpRequest &request);

private:
    size_t header_bytes_; ///< Number of bytes that belong to request line + headers.
    size_t body_bytes_;   ///< Content-Length after headers are parsed.
};

#endif
