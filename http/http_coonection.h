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
#include "http_types.h"
#include "http_routes.h"

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
    void init(int sockfd, const sockaddr_in &addr, int, int, string user, string password, string sqlname);

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
        return &req.m_address;
    }

    /**
     * @brief Initialize MySQL result set
     * @param conn_pool Database connection pool
     */
    void initmysql_result(DB_CONNECTION_POOL *conn_pool);

    int timer_flag; ///< Timer expiration flag
    int improv;     ///< Improv flag for connection state

private:
    /* Parser state */
    CHECK_STATE m_check_state; ///< Current parsing state
    int cgi;                   ///< CGI flag

    /* Configuration */
    map<string, string> m_users; ///< User credentials cache
    int m_trigger_mode;          ///< Event trigger mode

    /* Database credentials */
    char sql_user[100];     ///< Database username
    char sql_password[100]; ///< Database password
    char sql_name[100];     ///< Database name

    HttpRequest req;
    HttpResponse res;

    ROUTER &router = ROUTER::get_instance(); // route instance

private:
    /*Private Function*/

    void init(); ///< Internal initialization

    /**
     * @brief Process read buffer
     * @return HTTP_CODE processing result
     */
    HTTP_CODE process_read();

    HTTP_CODE parse_request_line(char *text); ///< Parse request line
    HTTP_CODE parse_headers(char *text);      ///< Parse headers
    HTTP_CODE parse_content(char *text);      ///< Parse content

    /**
     * @brief Get current line from buffer
     * @return Pointer to line start
     */
    char *get_line()
    {
        return req.m_read_buf + req.m_start_line;
    }

    /**
     * @brief Parse line from buffer
     * @return LINE_STATUS parsing result
     */
    LINE_STATUS parse_line();
};

#endif