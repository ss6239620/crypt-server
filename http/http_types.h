#ifndef HTTP_TYPES_H
#define HTTP_TYPES_H

#include <string>
#include <functional>
#include <map>
#include <sys/stat.h>
#include <netinet/in.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

#include "./jsonparser.h"
#include "../log/log.h"

static const int FILENAME_LEN = 200;       ///< Maximum length for file paths
static const int READ_BUFFER_SIZE = 2048;  ///< Size of read buffer
static const int WRITE_BUFFER_SIZE = 1024; ///< Size of write buffer

static int m_close_log;

/**
 * @enum METHOD
 * @brief Supported HTTP methods
 */
enum METHOD
{
    GET = 0, ///< HTTP GET method
    POST,    ///< HTTP POST method
    HEAD,    ///< HTTP HEAD method
    PUT,     ///< HTTP PUT method
    DELETE,  ///< HTTP DELETE method
    TRACE,   ///< HTTP TRACE method
    OPTIONS, ///< HTTP OPTIONS method
    CONNECT, ///< HTTP CONNECT method
    PATH     ///< PATH method
};

// Simple request and response classes.
class HttpRequest
{
public:
    METHOD m_method; ///< HTTP method
    std::string path;
    sockaddr_in m_address;             ///< Client address
    char m_read_buf[READ_BUFFER_SIZE]; ///< Read buffer
    long m_read_idx;                   ///< Read buffer index
    long m_checked_idx;                ///< Checked position in buffer
    int m_start_line;                  ///< Start line position
    char *m_url;                       ///< Request URL
    char *m_version;                   ///< HTTP version
    char *m_host;                      ///< Host header
    long m_content_length;             ///< Content length
    bool m_linger;                     ///< Keep-alive flag
    int m_sockfd;                      ///< Client socket descriptor

    JSON m_body;

    // Add more request properties as needed...

    // std::string get_header(const std::string &index) const
    // {
    //     auto it = headers.find(index);
    //     return it != headers.end() ? it->second : "";
    // }
};

class HttpResponse
{
public:
    char m_write_buf[WRITE_BUFFER_SIZE]; ///< Write buffer
    int m_write_idx;                     ///< Write buffer index
    char m_real_file[FILENAME_LEN];      ///< Requested file path
    /* File handling */
    char *m_file_address;    ///< Mapped file address
    struct stat m_file_stat; ///< File status
    struct iovec m_iv[2];    ///< I/O vector for writev
    int m_iv_count;          ///< I/O vector count
    char *doc_root;          ///< Document root directory

    /* CGI and database */
    char *m_string;      ///< String storage
    int bytes_to_send;   ///< Bytes remaining to send
    int bytes_have_send; ///< Bytes already sent

    bool m_linger;

    int m_close_log; ///< Logging control flag

    sockaddr_in m_address; ///< Client address

private:
    /**
     * @brief Get status message for HTTP status code
     */
    const char *get_status_message(int status)
    {
        switch (status)
        {
        case 200:
            return "OK";
        case 400:
            return "Bad Request";
        case 404:
            return "Not Found";
        case 500:
            return "Internal Server Error";
        default:
            return "Unknown Status";
        }
    }
    bool mapfile(const char *file_name);

public:
    /**
     * @brief Send plain text response to the client
     * @param status HTTP status code (e.g., 200, 404)
     * @param content_type MIME type of the content (e.g., "text/plain")
     * @param content The actual content to send
     * @return true if successful, false otherwise
     */
    bool send(int status, const std::string &content);

    bool render(int status, const std::string &file_name);

    /**
     * @brief Unmap memory-mapped file
     */
    void unmap();

    /* Response generation methods */
    bool add_response(const char *format, ...);          ///< Add formatted response
    bool add_content(const char *content);               ///< Add content to response
    bool add_status_line(int status, const char *title); ///< Add status line
    bool add_headers(int content_length);                ///< Add response headers
    bool add_content_type(const char *type);             ///< Add Content-Type header
    bool add_content_length(int content_length);         ///< Add Content-Length header
    bool add_linger();                                   ///< Add Connection header
    bool add_blank_line();                               ///< Add CRLF to response
};

#endif