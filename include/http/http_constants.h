#ifndef HTTP_CONSTANTS_H
#define HTTP_CONSTANTS_H

#include <cstddef>

static const size_t HTTP_READ_CHUNK_SIZE = 4096;       ///< Bytes attempted from recv() at one time.
static const size_t HTTP_MAX_HEADER_BYTES = 16 * 1024; ///< Max bytes before the blank line that ends headers.
static const size_t HTTP_MAX_BODY_BYTES = 1024 * 1024; ///< Max request body accepted by this demo server.

/**
 * @enum METHOD
 * @brief Supported HTTP methods.
 */
enum METHOD
{
    GET = 0, ///< HTTP GET method.
    POST,    ///< HTTP POST method.
    HEAD,    ///< HTTP HEAD method.
    PUT,     ///< HTTP PUT method.
    DELETE,  ///< HTTP DELETE method.
    TRACE,   ///< HTTP TRACE method.
    OPTIONS, ///< HTTP OPTIONS method.
    CONNECT, ///< HTTP CONNECT method.
    PATCH,   ///< HTTP PATCH method.
    UNKNOWN  ///< Method token was not recognized.
};

#endif
