#include "webserver.h"

WEBSERVER::WEBSERVER()
{
    users = new HTTP_CONN[MAX_FD];

    // find server path and store it to sting and add /root at last of string and then store it to m_root
    char server_path[200];
    getcwd(server_path, 200);
    char root[6] = "/root";
    m_root = (char *)malloc(strlen(server_path) + strlen(root) + 1);
    strcpy(m_root, server_path);
    strcat(m_root, root);

    users_timer = new client_data[MAX_FD];
}

WEBSERVER::~WEBSERVER()
{
    close(m_epollfd);
    close(m_listenfd);
    close(m_pipefd[1]); // 1 for write
    close(m_pipefd[0]); // 0 for read
    delete[] users;
    delete[] users_timer;
    delete m_pool;
}

void WEBSERVER::init(int port, string user, string password, string dbname, int log_write, int opt_linger, int trigger_mode, int sql_num, int thread_num, int close_log, int actor_model)
{
    m_port = port;
    m_user = user;
    m_password = password,
    m_dbname = dbname;
    m_log_write = log_write;
    m_opt_linger = opt_linger;
    m_trigger_mode = trigger_mode;
    m_sql_num = sql_num;
    m_thread_num = thread_num;
    m_close_log = close_log;
    m_actor_mode = actor_model;
}

void WEBSERVER::trigger_mode()
{
    // LT + LT
    if (m_trigger_mode == 0)
    {
        m_listen_trigger_mode = 0;
        m_conn_trigger_mode = 0;
    }
    // LT + ET
    else if (m_trigger_mode == 1)
    {
        m_listen_trigger_mode = 0;
        m_conn_trigger_mode = 1;
    }
    // ET + LT
    else if (m_trigger_mode == 2)
    {
        m_listen_trigger_mode = 1;
        m_conn_trigger_mode = 0;
    }
    // ET + ET
    else if (m_trigger_mode == 3)
    {
        m_listen_trigger_mode = 1;
        m_conn_trigger_mode = 1;
    }
}

void WEBSERVER::log_write()
{
    if (m_close_log == 0)
    {
        if (m_log_write == 1) // if log mode is asynchronous
            LOG::get_instance()->init("./ServerLog", m_close_log, 2000, 800000, 800);
        else
            LOG::get_instance()->init("./ServerLog", m_close_log, 2000, 800000, 0);
    }
}

void WEBSERVER::sql_pool()
{
    m_connpool = DB_CONNECTION_POOL::get_instance();
    m_connpool->init("sql12.freesqldatabase.com", m_user, m_password, m_dbname, 3306, m_sql_num, m_close_log);
    // store the username and password in hashmap
    users->initmysql_result(m_connpool);
}

void WEBSERVER::thread_pool()
{
    m_pool = new THREADPOOL<HTTP_CONN>(m_actor_mode, m_connpool, m_thread_num);
}

/**
 * DESC: The function eventListen() runs only once when the server starts.
 * 1) It sets up the server's listening socket, epoll instance, and signal handlers.
 * 2) It does not handle client connections directly. Instead, it prepares the server to handle events.
 */

void WEBSERVER::event_listen()
{
    // This socket will listen for incoming connections
    m_listenfd = socket(PF_INET, SOCK_STREAM, 0);

    if (m_opt_linger == 0)
    {
        struct linger tmp = {0, 1};
        setsockopt(m_listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
    }
    // if m_opt_linger is true make socket wait for few second
    else if (m_opt_linger == 0)
    {
        struct linger tmp = {1, 1};
        setsockopt(m_listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
    }
    int ret = 0;
    struct sockaddr_in address;
    bzero(&address, sizeof(address));

    address.sin_family = AF_INET; // AF_INET->ipv4 address
    // accept connection from any available interface htonl() (Host to Network Long)
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(m_port); // store port number htons() (Host to Network Short)

    // When restarting a server quickly without waiting for old sockets to be released.
    int flag = 1;
    setsockopt(m_listenfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));

    // Give the socket FD the local address ready the server for handling new connection
    ret = bind(m_listenfd, (struct sockaddr *)&address, sizeof(address));
    assert(ret >= 0);
    /* Prepare to accept connections on socket FD.
    PARAMETRES:
    1) The socket file descriptor m_listenfd that has been created and bound to an address using bind().
    2) The maximum number of pending connections that can be queued before accept() is called.
    */
    ret = listen(m_listenfd, 5);
    assert(ret >= 0);

    utils.init(TIMESLOT); // set the timeslot

    // epoll handling
    epoll_event events[MAX_EVENT_NUMBER]; // Array to store triggered events
    m_epollfd = epoll_create(5);          // Create an epoll instance 5 is of no use here
    assert(m_epollfd != -1);              // Ensure epoll instance was created successfully

    utils.addfd(m_epollfd, m_listenfd, false, m_listen_trigger_mode);
    HTTP_CONN::m_epollfd = m_epollfd;

    /*
    here m_pipefd is used for signal processing it is used for local communication within unix system it create two pair od socket so any thread can notify the main thread if it want to.
    Example: One thread writes "SHUTDOWN" to m_pipefd[1], the other thread reads it from m_pipefd[0] and exits.

    As We know that Signals (e.g., SIGTERM, SIGHUP) cannot be directly handled inside epoll_wait().Instead, the signal handler writes to m_pipefd[1], and m_pipefd[0] is monitored by epoll. This ensures safe signal handling without race conditions.
    */
    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, m_pipefd);
    assert(ret != -1);
    // make this event non_blocking as it does not have to wait for other process
    utils.set_non_blocking(m_pipefd[1]);           // make the write end of pipe non_blocking
    utils.addfd(m_epollfd, m_pipefd[0], false, 0); // add the read end of pipe to epoll

    // adding signals for Handling timeouts and graceful shutdown.
    utils.addsig(SIGALRM, utils.sig_handler, false);
    utils.addsig(SIGTERM, utils.sig_handler, false);

    alarm(TIMESLOT); // setting up alarm

    UTILS::u_pipefd = m_pipefd;
    UTILS::u_epollfd = m_epollfd;
}

/**
 * Here we store client data for funthur use and intialize a new timer add client data address to timer
 * as a double link and add the new connection timer to timer linked list.
 */

void WEBSERVER::timer(int connfd, struct sockaddr_in client_address)
{
    users[connfd].init(connfd, client_address, m_root, m_conn_trigger_mode, m_close_log, m_user, m_password, m_dbname); // intialize new connection

    users_timer[connfd].address = client_address;
    users_timer[connfd].sockfd = connfd;
    UTIL_TIMER *timer = new UTIL_TIMER;
    timer->user_data = &users_timer[connfd];
    timer->cb_func = cb_func;
    time_t cur = time(NULL);            // get current time
    timer->expire = cur + 3 * TIMESLOT; // make the expiration time to currently 3*time_slot=15 seconds
    users_timer[connfd].timer = timer;
    utils.m_timer_lst.add_timer(timer); // add the connection timer to the list
}

void WEBSERVER::adjust_timer(UTIL_TIMER *timer)
{
    time_t cur = time(NULL);            // get the current time
    timer->expire = cur + 3 * TIMESLOT; // add new expiration time
    utils.m_timer_lst.adjust_timer(timer);

    LOG_INFO("%s", "adjust timer once.");
}

void WEBSERVER::deal_timer(UTIL_TIMER *timer, int sockfd)
{
    timer->cb_func(&users_timer[sockfd]); // just execute cb_func and close the connection
    if (timer)
        utils.m_timer_lst.delete_timer(timer); // delete the timer from list too
    LOG_INFO("close fd %d", users_timer[sockfd].sockfd);
}

bool WEBSERVER::deal_client_data()
{
    struct sockaddr_in client_address;
    socklen_t client_addrlen = sizeof(client_address);
    // bool accepted = false;  // Flag to track if at least one client was accepted

    // listen mode in LT in LT we can only accept single client at a time
    if (m_listen_trigger_mode == 0)
    {
        // accept the new client connection and get their fd
        int connfd = accept(m_listenfd, (struct sockaddr *)&client_address, &client_addrlen);
        if (connfd < 0)
        {
            LOG_ERROR("%s:errno is:%d", "accept error", errno);
            return false;
        }
        if (HTTP_CONN::m_user_count >= MAX_FD) // if user count is more than MAX_FD show error
        {
            utils.show_error(connfd, "INTERNAL SERVER BUSY");
            LOG_ERROR("%s", "INTERNAL SERVER BUSY");
            return false;
        }
        // add new connection to timer for monitoring
        timer(connfd, client_address);
    }
    // listen mode in ET we have to accept all the client that are eaiting for us
    else
    {
        while (1)
        {
            // accept the new client connection and get their fd
            int connfd = accept(m_listenfd, (struct sockaddr *)&client_address, &client_addrlen);
            if (connfd < 0)
            {
                LOG_ERROR("%s:errno is:%d", "accept error", errno);
                break;
            }
            if (HTTP_CONN::m_user_count >= MAX_FD) // if user count is more than MAX_FD show error
            {
                utils.show_error(connfd, "INTERNAL SERVER BUSY");
                LOG_ERROR("%s", "INTERNAL SERVER BUSY");
                break;
            }
            timer(connfd, client_address);
            // accepted = true;  // A client was accepted successfully
        }
        // return accepted;
        return false;
    }
    return true;
}

/**
 * this function handles signal when signal arrive from pipe it is sotred in buffer and then
 * we loop it find the signal through number in signals array and the handle timerout and
 * server
 */

bool WEBSERVER::deal_with_signal(bool &timeout, bool &stop_server)
{
    int ret = 0;
    char signals[1024];
    /**
     * read signal number at read end it can accept multiple signal at once ret has number of
     * byte received.
     * It is stored as a int value so signals[0]=14 and so on
     */
    ret = recv(m_pipefd[0], signals, sizeof(signals), 0);

    if (ret == -1)
        return false;

    else if (ret == 0)
        return false;

    else
    {
        for (int i = 0; i < ret; i++)
        {
            switch (signals[i])
            {
            case SIGALRM: // 14   (SIGALRM)
            {
                timeout = true;
                break;
            }

            case SIGTERM: // 15   (SIGTERM)
            {
                stop_server = true;
                break;
            }
            }
        }
    }
    return true;
}

void WEBSERVER::deal_with_read(int sockfd)
{
    UTIL_TIMER *timer = users_timer[sockfd].timer; // timer of client conn

    // REACTOR MODE(SYNCRONOUS MODE)
    if (m_actor_mode == 1)
    {
        if (timer)
            adjust_timer(timer); // adjust timer

        // we can find current client connfd by moving in array. Append the new work to the worker queue  for worker thread to consume.
        m_pool->append(users + sockfd, 0);

        while (true) // wait till a worker thread consumed client data.
        {
            if (users[sockfd].improv == 1) // if the data of client is proccessd by thread pool
            {
                if (users[sockfd].timer_flag == 1) // flag for timer cleanup of resources
                {
                    deal_timer(timer, sockfd);    // delete from timer and release associated resource.
                    users[sockfd].timer_flag = 0; // reset flag.
                }
            }
            users[sockfd].improv = 0; // reset flag
            break;
        }
    }
    // PROACTOR MODE (ASYNCYRONOUS MODE)
    else
    {
        // Here wroker thread do not read data it just acquire db connection and process it.
        if (users[sockfd].read_once())
        {
            // cleint ip
            LOG_INFO("deal with the client(%s)", inet_ntoa(users[sockfd].get_address()->sin_addr));

            m_pool->append_p(users + sockfd); // append to proactor mode

            if (timer) // adjust expiration time
                adjust_timer(timer);
        }
        else
            deal_timer(timer, sockfd); // remove from timer list and release resource
    }
}

void WEBSERVER::deal_with_write(int sockfd)
{
    UTIL_TIMER *timer = users_timer[sockfd].timer;

    // REACTOR MODE(SYNCRONOUS MODE)
    if (m_actor_mode == 1)
    {
        if (timer)
            adjust_timer(timer);

        // we can find current client connfd by moving in array. Append the new work to the worker queue  for worker thread to consume.
        m_pool->append(users + sockfd, 1); // 1 for write

        while (true) // wait till a worker thread consumed client data.
        {
            if (users[sockfd].improv) // request is processed
            {
                if (users[sockfd].timer_flag == 1) // flag for timer cleanup of resources
                {
                    deal_timer(timer, sockfd);    // delete from timer and release associated resource.
                    users[sockfd].timer_flag = 0; // reset flag.
                }
                users[sockfd].improv = 0;
                break;
            }
        }
    }
    // PROACTOR MODE (ASYNCYRONOUS MODE)
    else
    {
        if (users[sockfd].write())
        {
            LOG_INFO("send data to the client(%s)", inet_ntoa(users[sockfd].get_address()->sin_addr));

            if (timer)
                adjust_timer(timer); // adjust expiration time
        }
        else
        {
            deal_timer(timer, sockfd); // remove from timer list and release resource
        }
    }
}

void WEBSERVER::event_loop()
{
    bool timeout = false;     // for removing timer from timer list
    bool stop_server = false; // for stoping server when SIGTERM arrive.

    while (!stop_server)
    {
        // wait for data to arrive from client in epoll fds event are already added when client connect see HTTP_CONN::init() function. hover on epoll_wait to get info about func.
        int number = epoll_wait(m_epollfd, events, MAX_EVENT_NUMBER, -1);
        // EINTR means:if any of the registerd event has occured check if the errno is EINTR otherwise break the loop
        if (number < 0 && errno != EINTR)
        {
            LOG_ERROR("%s", "epoll failure");
            break;
        }
        // loop all the epoll which has some event occured on them dosent matter if it is signal,read or write
        for (int i = 0; i < number; i++)
        {
            int sockfd = events[i].data.fd; // take out individual event fd

            // if server epoll instance has some event likely arrival of new connection handle it.
            if (sockfd == m_listenfd)
            {
                bool flag = deal_client_data();
                if (flag == false)
                    continue;
            }
            /*
            This condition happen when the client drop the connection or some error occured on epoll fd

            EPOLLRDHUP:This event is triggered when the remote peer closes the connection or performs a shutdown for writing

            EPOLLHUP:This event occurs when the file descriptor is hung up.
            Typically happens when both reading and writing ends are closed.


            EPOLLERR:This event indicates an error condition on the file descriptor.
            */
            else if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR))
            {
                UTIL_TIMER *timer = users_timer[sockfd].timer; // take out the timer
                deal_timer(timer, sockfd);                     // release resources
            }

            /*
            Here we deal with the signal both signal  SIGALARM and SIGTERM(ctrl+c) are dealth here.
             EPOLLIN: The associated file is available for read(2) operations.
             EPOLLOUT: The associated file is available for write(2) operations.
            */
            else if ((sockfd == m_pipefd[0]) && (events[i].events & EPOLLIN))
            {
                bool flag = deal_with_signal(timeout, stop_server);
                if (flag == false)
                    LOG_ERROR("%s", "Failure dealing with signals.");
            }
            // when client send some data to read
            else if (events[i].events & EPOLLIN)
            {
                deal_with_read(sockfd);
            }
            // when server is ready to send response to server
            else if (events[i].events & EPOLLOUT)
            {
                deal_with_write(sockfd);
            }
        }
        if (timeout)
        {
            utils.time_handler(); // process all timer and sets alarm

            LOG_INFO("%s", "timer tick");
            timeout = false;
        }
    }
}