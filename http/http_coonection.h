#ifndef _HTTPCONNECTION_H_
#define _HTTPCONNECTION_H_

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <map>

#include "../lock/locker.h"
#include "../cgi_mysql/connection_pool.h"
#include "../timer/timer.h"
#include "../log/log.h"

/**
 * @class HTTP_CONN
 * @brief Handles HTTP client connections including request processing,
 *        response generation, and database integration
 */
class HTTP_CONN
{
public:
    /*Public enums*/

    static const int FILENAME_LEN = 200;       ///< Maximum length for file paths
    static const int READ_BUFFER_SIZE = 2048;  ///< Size of read buffer
    static const int WRITE_BUFFER_SIZE = 1024; ///< Size of write buffer

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

    /**
     * @enum CHECK_STATE
     * @brief Parser state machine states
     */
    enum CHECK_STATE
    {
        CHECK_STATE_REQUESTLINE = 0, ///< Parsing request line
        CHECK_STATE_HEADER,          ///< Parsing headers
        CHECK_STATE_CONTENT          ///< Parsing content
    };

    /**
     * @enum HTTP_CODE
     * @brief HTTP processing results
     */
    enum HTTP_CODE
    {
        NO_REQUEST,        ///< Incomplete request
        GET_REQUEST,       ///< Valid GET request
        BAD_REQUEST,       ///< Malformed request
        NO_RESOURCE,       ///< Resource not found
        FORBIDDEN_REQUEST, ///< Forbidden resource
        FILE_REQUEST,      ///< Valid file request
        INTERNAL_ERROR,    ///< Server error
        CLOSED_CONNECTION  ///< Connection closed
    };

    /**
     * @enum LINE_STATUS
     * @brief Request line parsing status
    */
    enum LINE_STATUS
    {
        LINE_OK = 0, ///< Line parsed successfully
        LINE_BAD,    ///< Malformed line
        LINE_OPEN    ///< Incomplete line
    };

public:
    /*Public Variable*/

    static int m_epollfd;    ///< Epoll file descriptor
    static int m_user_count; ///< Count of active connections
    MYSQL *mysql;            ///< MySQL connection handle
    int m_state;             ///< 0 = read, 1 = write

public:
    /*Public Function*/

    HTTP_CONN() {}  ///< Default constructor
    ~HTTP_CONN() {} ///< Destructor

    /**
     * @brief Initialize connection
     * @param sockfd Client socket descriptor
     * @param addr Client address structure
     * @param root Document root directory
     * @param trigger_mode Event trigger mode
     * @param close_log Logging flag
     * @param user Database username
     * @param password Database password
     * @param sqlname Database name
     */
    void init(int sockfd, const sockaddr_in &addr, char *, int, int, string user, string password, string sqlname);

    /**
     * @brief Close connection
     * @param real_close Whether to actually close socket
     */
    void close_conn(bool real_close = true);

    /**
     * @brief Main processing method
     */
    void process();

    /**
     * @brief Read data from socket
     * @return true if read succeeded, false otherwise
     */
    bool read_once();

    /**
     * @brief Write data to socket
     * @return true if write succeeded, false otherwise
     */
    bool write();

    /**
     * @brief Get client address
     * @return Pointer to sockaddr_in structure
     */
    sockaddr_in *get_address()
    {
        return &m_address;
    }

    /**
     * @brief Initialize MySQL result set
     * @param conn_pool Database connection pool
     */
    void initmysql_result(DB_CONNECTION_POOL *conn_pool);

    int timer_flag; ///< Timer expiration flag
    int improv;     ///< Improv flag for connection state

private:
    /* Connection data */
    int m_sockfd;                        ///< Client socket descriptor
    sockaddr_in m_address;               ///< Client address
    char m_read_buf[READ_BUFFER_SIZE];   ///< Read buffer
    long m_read_idx;                     ///< Read buffer index
    long m_checked_idx;                  ///< Checked position in buffer
    int m_start_line;                    ///< Start line position
    char m_write_buf[WRITE_BUFFER_SIZE]; ///< Write buffer
    int m_write_idx;                     ///< Write buffer index

    /* Parser state */
    CHECK_STATE m_check_state;      ///< Current parsing state
    METHOD m_method;                ///< HTTP method
    char m_real_file[FILENAME_LEN]; ///< Requested file path
    char *m_url;                    ///< Request URL
    char *m_version;                ///< HTTP version
    char *m_host;                   ///< Host header
    long m_content_length;          ///< Content length
    bool m_linger;                  ///< Keep-alive flag

    /* File handling */
    char *m_file_address;    ///< Mapped file address
    struct stat m_file_stat; ///< File status
    struct iovec m_iv[2];    ///< I/O vector for writev
    int m_iv_count;          ///< I/O vector count

    /* CGI and database */
    int cgi;             ///< CGI flag
    char *m_string;      ///< String storage
    int bytes_to_send;   ///< Bytes remaining to send
    int bytes_have_send; ///< Bytes already sent

    /* Configuration */
    char *doc_root;              ///< Document root directory
    map<string, string> m_users; ///< User credentials cache
    int m_trigger_mode;          ///< Event trigger mode
    int m_close_log;             ///< Logging control flag

    /* Database credentials */
    char sql_user[100];     ///< Database username
    char sql_password[100]; ///< Database password
    char sql_name[100];     ///< Database name

private:
    /*Private Function*/

    void init(); ///< Internal initialization

    /**
     * @brief Process read buffer
     * @return HTTP_CODE processing result
     */
    HTTP_CODE process_read();

    /**
     * @brief Process write operation
     * @param ret HTTP_CODE from process_read
     * @return true if write should continue, false otherwise
     */
    bool process_write(HTTP_CODE ret);

    HTTP_CODE parse_request_line(char *text); ///< Parse request line
    HTTP_CODE parse_headers(char *text);      ///< Parse headers
    HTTP_CODE parse_content(char *text);      ///< Parse content
    HTTP_CODE do_request();                   ///< Handle valid request

    /**
     * @brief Get current line from buffer
     * @return Pointer to line start
     */
    char *get_line()
    {
        return m_read_buf + m_start_line;
    }

    /**
     * @brief Parse line from buffer
     * @return LINE_STATUS parsing result
     */
    LINE_STATUS parse_line();

    /**
     * @brief Unmap memory-mapped file
     */
    void unmap();

   /* Response generation methods */
    bool add_response(const char *format, ...);   ///< Add formatted response
    bool add_content(const char *content);        ///< Add content to response
    bool add_status_line(int status, const char *title); ///< Add status line
    bool add_headers(int content_length);        ///< Add response headers
    bool add_content_type();                     ///< Add Content-Type header
    bool add_content_length(int content_length); ///< Add Content-Length header
    bool add_linger();                          ///< Add Connection header
    bool add_blank_line();                      ///< Add CRLF to response
};

#endif