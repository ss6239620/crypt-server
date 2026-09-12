#ifndef _HTTPCONNECTION_H_
#define _HTTPCONNECTION_H_

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <map>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sys/epoll.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <unistd.h>

#include "../cgi_mysql/connection_pool.h"
#include "../lock/locker.h"
#include "../log/log.h"
#include "../timer/timer.h"
#include "http_request_parser.h"
#include "http_response.h"
#include "http_routes.h"

/**
 * @class HTTP_CONN
 * @brief Handles one client socket and owns that socket's HTTP state.
 */
class HTTP_CONN
{
public:
    /**
     * @enum HTTP_CODE
     * @brief Internal result used after trying to parse the read buffer.
     */
    enum HTTP_CODE
    {
        NO_REQUEST,         ///< Incomplete request.
        GET_REQUEST,        ///< Valid full request, even when the method is POST.
        BAD_REQUEST,        ///< Malformed request.
        METHOD_NOT_ALLOWED, ///< Method is valid HTTP but not supported.
        PAYLOAD_TOO_LARGE,  ///< Header/body size limit was exceeded.
        INTERNAL_ERROR,     ///< Server error.
        CLOSED_CONNECTION   ///< Connection closed.
    };

    static int m_epollfd;    ///< Epoll file descriptor shared by all connections.
    static int m_user_count; ///< Count of active connections.
    MYSQL *mysql;            ///< MySQL connection handle.
    int m_state;             ///< Actor-mode flag: 0 = read task, 1 = write task.

    HTTP_CONN();  ///< Default constructor.
    ~HTTP_CONN(); ///< Destructor.

    /**
     * @brief Attach this HTTP_CONN object to a newly accepted client socket.
     */
    void init(int sockfd, const sockaddr_in &addr, int, int, std::string user, std::string password, std::string sqlname);

    /**
     * @brief Close connection.
     * @param real_close Whether to actually close socket.
     */
    void close_conn(bool real_close = true);

    /**
     * @brief Parse the current buffer and build a response when a full request exists.
     */
    void process();

    /**
     * @brief Read available bytes from the non-blocking socket into m_read_buffer.
     */
    bool read_once();

    /**
     * @brief Send response bytes using writev().
     */
    bool write();

    /**
     * @brief Get client address.
     */
    sockaddr_in *get_address()
    {
        return &m_address;
    }

    /**
     * @brief Initialize MySQL result set.
     */
    void initmysql_result(DB_CONNECTION_POOL *conn_pool);

    int timer_flag; ///< Timer expiration flag.
    int improv;     ///< Improv flag for connection state.

private:
    std::map<std::string, std::string> m_users; ///< User credentials cache.
    int m_trigger_mode;          ///< Event trigger mode for this client socket.

    char sql_user[100];     ///< Database username.
    char sql_password[100]; ///< Database password.
    char sql_name[100];     ///< Database name.

    int m_sockfd;              ///< Client socket descriptor owned by this connection.
    sockaddr_in m_address;     ///< Client address owned by this connection.
    std::string m_read_buffer; ///< Bytes read from recv() until one full HTTP request is parsed.
    size_t m_write_offset;     ///< Total response bytes already written to the socket.
    HTTP_CODE m_read_error;    ///< Deferred read-size error that should become an HTTP response.
    HttpRequestParser parser;  ///< Converts m_read_buffer into req.
    HttpRequest req;
    HttpResponse res;

    ROUTER &router; ///< Router singleton used after a request is parsed.

    /**
     * @brief Reset one request/response transaction while keeping the socket open.
     */
    void init();

    /**
     * @brief Process read buffer.
     */
    HTTP_CODE process_read();

    /**
     * @brief Build an error response without calling user route code.
     */
    void make_error_response(HTTP_CODE code);
};

#endif
