#ifndef HTTP_REQUEST_H
#define HTTP_REQUEST_H

#include "http_constants.h"
#include "jsonparser.h"

#include <map>
#include <netinet/in.h>
#include <string>

/**
 * @class HttpRequest
 * @brief Parsed HTTP request data used by route handlers.
 *
 * The socket read buffer lives in HTTP_CONN. This class only stores values that
 * were parsed out of the bytes, so handlers do not depend on temporary char*
 * pointers inside a mutable receive buffer.
 */
class HttpRequest
{
public:
    METHOD method_type;                         ///< Enum version of the HTTP method.
    std::string method;                         ///< Original method text, e.g. "GET".
    std::string target;                         ///< Full request target, e.g. "/path?x=1".
    std::string path;                           ///< Normalized path without query string.
    std::string query;                          ///< Query string without the leading '?'.
    std::string version;                        ///< HTTP version string, e.g. "HTTP/1.1".
    std::string host;                           ///< Value from the Host header.
    std::map<std::string, std::string> headers; ///< Header names are stored lower-case for lookup.
    std::string body;                           ///< Raw body bytes; form bodies stay raw here.
    size_t content_length;                      ///< Parsed Content-Length value.
    bool keep_alive;                            ///< true means the connection may be reused.
    sockaddr_in client_address;                 ///< Client address copied in for handler context.
    JSON json_body;                             ///< Parsed JSON body when Content-Type is application/json.

    HttpRequest();

    /**
     * @brief Reset parsed fields before reading the next keep-alive request.
     */
    void reset();

    /**
     * @brief Case-insensitive header lookup.
     * @param name Header name in any case, e.g. "Content-Type".
     * @return Header value, or an empty string when it is missing.
     */
    std::string get_header(const std::string &name) const;

    /**
     * @brief Whether the request contains the named header.
     */
    bool has_header(const std::string &name) const;
};

#endif
