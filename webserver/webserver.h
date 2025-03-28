/**
 * Here SERVER_FLOW:
 * '''
 * sequenceDiagram
    Server->>WEBSERVER: init()
    WEBSERVER->>ThreadPool: Create workers
    WEBSERVER->>DB_Pool: Create connections
    WEBSERVER->>Epoll: Setup listener
    loop Event Loop
        WEBSERVER->>Epoll: Wait for events
        alt New Connection
            WEBSERVER->>WEBSERVER: timer()
        else Data Ready
            WEBSERVER->>ThreadPool: dispatch task
        end
    end
 * '''
 *
 */

#ifndef _WEBSERVER_H_
#define _WEBSERVER_H_

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <cassert>
#include <sys/epoll.h>

#include "../threadpool/threadpool.h"
#include "../http/http_coonection.h"

/**
 * @def MAX_FD
 * @brief Maximum number of file descriptors
 */
const int MAX_FD = 65536;

/**
 * @def MAX_EVENT_NUMBER
 * @brief Maximum epoll events to process at once
 */
const int MAX_EVENT_NUMBER = 10000;

/**
 * @def TIMESLOT
 * @brief Default timeout duration (seconds) for connections
 */
const int TIMESLOT = 5;

/**
 * @class WEBSERVER
 * @brief Main web server class implementing:
 * - Event-driven architecture with epoll
 * - Thread pool for request processing
 * - Connection pooling for MySQL
 * - Timer-based connection management
 * - Configurable trigger modes
 */
class WEBSERVER
{
public:
    WEBSERVER();  ///< Default constructor
    ~WEBSERVER(); ///< Destructor (cleans up resources)

    /**
     * @brief Initialize server configuration
     * @param port Listening port number
     * @param user Database username
     * @param password Database password
     * @param dbname Database name
     * @param log_write Logging mode (0: synchronous, 1: asynchronous)
     * @param opt_linger Linger option for sockets
     * @param trigger_mode Event trigger mode
     * @param sql_num Database connection pool size
     * @param thread_num Thread pool size
     * @param close_log Disable logging if non-zero
     * @param actor_model 0: Proactor, 1: Reactor
     */
    void init(int port, string user, string password, string dbname, int log_write, int opt_linger, int trigger_mode, int sql_num, int thread_num, int close_log, int actor_model);

    ///< Initialize thread pool
    void thread_pool();

    ///< Initialize database connection pool
    void sql_pool();

    ///< Initialize logging system
    void log_write();

    ///< Configure event trigger modes
    void trigger_mode();

    ///< Set up listening socket and epoll
    void event_listen();

    ///< Main event processing loop
    void event_loop();

    /**
     * @brief Create timer for new connection
     * @param connfd Client socket descriptor
     * @param client_address Client address info
     */
    void timer(int connfd, struct sockaddr_in client_address);

    /**
     * @brief Adjust timer for existing connection
     * @param timer Timer object to adjust
     */
    void adjust_timer(UTIL_TIMER *timer);

    /**
     * @brief Handle expired timer
     * @param timer Expired timer object
     * @param sockfd Associated socket descriptor
     */
    void deal_timer(UTIL_TIMER *timer, int sockfd);

    ///< Process new client connections
    bool deal_client_data();

    ///< Handle signals
    bool deal_with_signal(bool &timeout, bool &stop_server);

    ///< Process read events
    void deal_with_read(int sockfd);

    ///< Process write events
    void deal_with_write(int sockfd);

public:
    /* Configuration parameters */
    int m_port;       ///< Server listening port
    char *m_root;     ///< Document root directory
    int m_log_write;  ///< Logging mode flag
    int m_close_log;  ///< Logging enable/disable flag
    int m_actor_mode; ///< Concurrency model (0:Proactor, 1:Reactor)

    /* Event handling */
    int m_pipefd[2];  ///< Signal notification pipe
    int m_epollfd;    ///< Epoll instance file descriptor
    HTTP_CONN *users; ///< Array of HTTP connection objects

    /* Database */
    DB_CONNECTION_POOL *m_connpool; ///< Database connection pool
    string m_user;                  ///< Database username
    string m_password;              ///< Database password
    string m_dbname;                ///< Database name
    int m_sql_num;                  ///< Database connection pool size

    /* Thread pool */
    THREADPOOL<HTTP_CONN> *m_pool; ///< Thread pool instance
    int m_thread_num;              ///< Number of worker threads

    /* Epoll events */
    epoll_event events[MAX_EVENT_NUMBER]; ///< Epoll event buffer

    /* Socket management */
    int m_listenfd;            ///< Listening socket descriptor
    int m_opt_linger;          ///< SO_LINGER socket option
    int m_trigger_mode;        ///< Global trigger mode
    int m_listen_trigger_mode; ///< Listen socket trigger mode (Determines how the server handles incoming connections.)
    int m_conn_trigger_mode;   ///< Connection socket trigger mode (Determines how the server handles client requests.)

    /* Timer management */
    client_data *users_timer; ///< Array of client timer data
    UTILS utils;              ///< Timer utilities instance
};

#endif