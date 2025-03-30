/**
 * Edge-Triggered (ET) Mode DESC:
 * - Events are triggered only when the state of the file descriptor changes.
 * - If you don’t read all available data, you won’t get another event until more data arrives.
 * - Efficient but requires non-blocking I/O to avoid missing events.
 * - It can only work with non-blocking I/O it is very fast as it dosent have to resend event
 * @example
 *  A socket receives 10KB of data. If you only read 5KB, you won't get another
 *  notification until new data arrives.
 *
 *
 * Level-Triggered (LT) Mode DESC:
 * - Events are triggered as long as data is available.
 * - If you don’t read all data, the event keeps triggering until it’s fully processed.
 * - Easier to use but may result in higher CPU usage due to repeated notifications.
 * - Works with both Blocking & Non-Blocking it is slow
 * - Works fine with blocking sockets because the event will keep firing if there's unread data.
 * - Works with non-blocking sockets, but it may result in frequent polling. e.g EPOLLONESHOT
 *  @example
 *   A socket receives 10KB of data. If you read 5KB, you’ll keep getting notifications until all 10KB are read.
 *
 * IOVEC:
 * writev is a system call in C and C++ (on Unix-like systems) that allows writing data from
 * multiple buffers (scatter-gather I/O) into a file descriptor in a single operation. It is useful for
 * minimizing system calls and efficiently writing structured data.
 * @example:
 *    // Define buffers
    const char *buf1 = "Hello, ";
    const char *buf2 = "World!";

    struct iovec iov[2];

    iov[0].iov_base = (void*)buf1;
    iov[0].iov_len = 7;  // Length of "Hello, "

    iov[1].iov_base = (void*)buf2;
    iov[1].iov_len = 6;  // Length of "World!"
    ssize_t bytes_written = writev(fd, iov, 2);
 */

#include "http_coonection.h"

#include <mysql/mysql.h>
#include <fstream>
#include <iostream>

int set_non_blocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

void addfd(int epollfd, int fd, bool one_shot, int trigger_mode)
{
    epoll_event event;
    event.data.fd = fd;

    if (1 == trigger_mode)
        event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    else
        event.events = EPOLLIN | EPOLLRDHUP;

    if (one_shot)
        event.events |= EPOLLONESHOT;

    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    set_non_blocking(fd);
}

void removefd(int epollfd, int fd)
{
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
    close(fd);
}

void modfd(int epollfd, int fd, int ev, int trigger_mode)
{
    epoll_event event;
    event.data.fd = fd;

    if (1 == trigger_mode)
        event.events = ev | EPOLLONESHOT | EPOLLET | EPOLLRDHUP;
    else
        event.events = ev | EPOLLONESHOT | EPOLLRDHUP;

    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

int HTTP_CONN::m_user_count = 0;
int HTTP_CONN::m_epollfd = -1;

void HTTP_CONN::close_conn(bool real_close)
{
    if (real_close && (req.m_sockfd != -1)) // if sockfd is -1 than it that socket is already closed.
    {
        printf("close %d\n", req.m_sockfd);
        removefd(m_epollfd, req.m_sockfd);
        req.m_sockfd = -1;
        m_user_count--;
    }
}

void HTTP_CONN::init(int sockfd, const sockaddr_in &addr, int trigger_mode, int close_log, string user, string password, string sqlname)
{
    req.m_sockfd = sockfd;
    req.m_address = addr;

    addfd(m_epollfd, sockfd, true, trigger_mode);
    m_user_count++;

    if (router.isStatic())
        res.doc_root = router.root_path();
    m_trigger_mode = trigger_mode;
    m_close_log = close_log;

    // copy db creadential
    strcpy(sql_user, user.c_str());
    strcpy(sql_password, password.c_str());
    strcpy(sql_name, sqlname.c_str());

    init(); // intialize rest of the variable
}

void HTTP_CONN::init()
{
    mysql = NULL;
    res.bytes_to_send = 0;
    res.bytes_have_send = 0;
    m_check_state = CHECK_STATE_REQUESTLINE;
    req.m_linger = false;
    res.m_linger = false;
    req.m_method = GET;
    req.m_url = 0;
    req.m_version = 0;
    req.m_content_length = 0;
    req.m_host = 0;
    req.m_start_line = 0;
    req.m_checked_idx = 0;
    req.m_read_idx = 0;
    res.m_write_idx = 0;
    cgi = 0;
    m_state = 0;
    timer_flag = 0;
    improv = 0;

    memset(req.m_read_buf, '\0', READ_BUFFER_SIZE);
    memset(res.m_write_buf, '\0', WRITE_BUFFER_SIZE);
    memset(res.m_real_file, '\0', FILENAME_LEN);
}

bool HTTP_CONN::read_once()
{
    if (req.m_read_idx >= READ_BUFFER_SIZE)
        return false;

    int byte_read = 0;

    /*
      In LT mode, if there’s still data left, the event remains active in epoll and will be triggered again.
      The function doesn't need a loop because epoll_wait() will notify again if more data is available.
     */
    if (0 == m_trigger_mode)
    {
        byte_read = recv(req.m_sockfd, req.m_read_buf + req.m_read_idx, READ_BUFFER_SIZE - req.m_read_idx, 0);
        req.m_read_idx += byte_read;

        if (byte_read <= 0)
            return false;

        return true;
    }
    /*
    In ET mode, the event only triggers when new data arrives, not when there’s unread data.
    This is why we must drain the entire buffer in one go. hence we use while loop.
    */
    else
    {
        while (true) // Keep reading all available data
        {
            byte_read = recv(req.m_sockfd, req.m_read_buf + req.m_read_idx, READ_BUFFER_SIZE - req.m_read_idx, 0);

            if (byte_read == -1) // recv() error
            {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                    break; // No more data left to read -> exit loop

                return false; // Some other error occurred, close connection
            }
            // if bytes_read is 0 we have succesfully read the data in buffer hence we return and loop automatically break.
            else if (byte_read == 0) // Client closed the connection byte read is 0 when cliend closed connection
                return false;

            req.m_read_idx += byte_read;
        }
        return true; // Successfully read all available data
    }
}

HTTP_CONN::LINE_STATUS HTTP_CONN::parse_line()
{
    char temp;
    for (; req.m_checked_idx < req.m_read_idx; ++req.m_checked_idx)
    {
        temp = req.m_read_buf[req.m_checked_idx]; // read a charactor from buffer.
        if (temp == '\r')                         // we have reached end of curr line
        {
            // If '\r' is the last character read, we need more data
            if ((req.m_checked_idx + 1) == req.m_read_idx)
                return LINE_OPEN;
            // If the next charactor is new_line than we have reached end of current line we make both charctor as '\0'
            else if (req.m_read_buf[req.m_checked_idx + 1] == '\n')
            {
                req.m_read_buf[req.m_checked_idx++] = '\0';
                req.m_read_buf[req.m_checked_idx++] = '\0'; // move to the first charctor of next line.
                return LINE_OK;
            }
            return LINE_BAD;
        }
        // genearlly this case is not required as most system can uses '\r' but some legacy system use only '\n' instead of \r\n or some misformed request so we handle it using this
        else if (temp == '\n')
        {
            // chack if revious charctor is \r if it is replace both \r\n with \0\0
            if (req.m_checked_idx > 1 && req.m_read_buf[req.m_checked_idx - 1] == '\r')
            {
                req.m_read_buf[req.m_checked_idx - 1] = '\0';
                req.m_read_buf[req.m_checked_idx++] = '\0'; // move to the first charctor of next line.
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    return LINE_OPEN;
}

HTTP_CONN::HTTP_CODE HTTP_CONN::parse_request_line(char *text) // helps in parsing 1st request line
{
    req.m_url = strpbrk(text, " \t"); // Find the first space or tab
    if (!req.m_url)
        return BAD_REQUEST;
    *req.m_url++ = '\0'; // replace space with \0
    // text="GET\0/index.html HTTP/1.1\r\n and m_url="  /index.html HTTP/1.1\r\n"

    char *method = text;
    if (strcasecmp(method, "GET") == 0) // match found as "GET\0"
        req.m_method = GET;
    else if (strcasecmp(method, "POST") == 0)
    {
        req.m_method = POST;
        cgi = 1; // enable CGI
    }
    else
        return BAD_REQUEST;

    req.m_url += strspn(req.m_url, " \t"); // This line is used to skip leading spaces
    // m_url = "/index.html HTTP/1.1\r\n"

    req.m_version = strpbrk(req.m_url, " \t");
    // m_version = "  HTTP/1.1\r\n"

    if (!req.m_version)
        return BAD_REQUEST;
    *req.m_version++ = '\0';
    // m_url: "/index.html\0HTTP/1.1\r\n"

    req.m_version += strspn(req.m_version, " \t");
    // m_version = "HTTP/1.1\r\n"
    if (strcasecmp(req.m_version, "HTTP/1.1") != 0) // match found
        return BAD_REQUEST;

    // Handle URL Starting with http:// or https:// we make it jump to 7 or 8
    if (strncasecmp(req.m_url, "http://", 7) == 0)
    {
        req.m_url += 7;
        req.m_url = strchr(req.m_url, '/');
    }
    if (strncasecmp(req.m_url, "https://", 8) == 0)
    {
        req.m_url += 8;
        req.m_url = strchr(req.m_url, '/');
    }
    // check if m_url has / or not
    if (!req.m_url || req.m_url[0] != '/')
        return BAD_REQUEST;
    if (strlen(req.m_url) == 1)
        strcat(req.m_url, "judge.html");
    m_check_state = CHECK_STATE_HEADER;

    return NO_REQUEST;

    /*here is something intresting m_url at last will be this /index.html\0HTTP/1.1\r\n but it will be stored as /index.html\0 because of \0 but what about m_version=HTTP/1.1\r\n it still contain \r\n it will be processed by parse_line function.
     */
}

HTTP_CONN::HTTP_CODE HTTP_CONN::parse_headers(char *text)
{
    if (text[0] == '\0') // we have reached end of header as we encounterd space line
    {
        if (req.m_content_length != 0) // when we have some content
        {
            m_check_state = CHECK_STATE_CONTENT;
            return NO_REQUEST; // continue parsing
        }
        return GET_REQUEST; // dirsctly make request
    }
    else if (strncasecmp(text, "Connection:", 11) == 0)
    {
        text += 11;                  // skip 11 charactor
        text += strspn(text, " \t"); // skip leading spaces
        if (strcasecmp(text, "keep-alive") == 0)
        {
            req.m_linger = true;
            res.m_linger = true;
        }
    }
    else if (strncasecmp(text, "Content-length:", 15) == 0)
    {
        text += 15;
        text += strspn(text, " \t"); // skip leading zero
        req.m_content_length = atoi(text);
    }
    else if (strncasecmp(text, "Host:", 5) == 0)
    {
        text += 5;
        text += strspn(text, " \t");
        req.m_host = text;
    }
    else
    {
        LOG_INFO("Oops!! Unknown header: %s.", text);
    }

    return NO_REQUEST; // continue parsing
}

HTTP_CONN::HTTP_CODE HTTP_CONN::parse_content(char *text)
{
    // check if from m_check_idx + m_content_length we can react end of buffer
    if (req.m_read_idx >= (req.m_content_length + req.m_checked_idx))
    {
        std::string content(text, req.m_content_length);
        req.m_body = JSON::parse(content);
        return GET_REQUEST;
    }
    return NO_REQUEST;
}

HTTP_CONN::HTTP_CODE HTTP_CONN::process_read()
{
    LINE_STATUS line_status = LINE_OK;
    HTTP_CODE ret = NO_REQUEST;
    char *text = 0;

    while ((m_check_state == CHECK_STATE_CONTENT && line_status == LINE_OK) || ((line_status = parse_line()) == LINE_OK))
    {
        text = get_line();
        req.m_start_line = req.m_checked_idx;
        switch (m_check_state)
        {
        case CHECK_STATE_REQUESTLINE:
        {
            ret = parse_request_line(text);
            if (ret == BAD_REQUEST)
                return BAD_REQUEST;
            break;
        }
        case CHECK_STATE_HEADER:
        {
            ret = parse_headers(text);
            if (ret == BAD_REQUEST)
                return BAD_REQUEST;
            else if (ret == GET_REQUEST)
                return GET_REQUEST; // if it is get request fo request
            break;
        }
        case CHECK_STATE_CONTENT:
        {
            ret = parse_content(text);
            if (ret == GET_REQUEST)
                return GET_REQUEST; // after parsing content make request
            line_status = LINE_OPEN;
            break;
        }
        default:
            return INTERNAL_ERROR;
        }
    }
    return NO_REQUEST;
}

bool HTTP_CONN::write()
{
    int temp = 0;
    if (res.bytes_to_send == 0)
    {
        // EPOLLIN file descriptor is ready for read operation for response
        modfd(m_epollfd, req.m_sockfd, EPOLLIN, m_trigger_mode); // modify fs to listen again for output
        init();                                                  // reintialize everything
        return true;
    }

    while (1)
    {
        // with writev we can write on multiple buffer it returns number of bytes written on error -1
        temp = writev(req.m_sockfd, res.m_iv, res.m_iv_count);
        if (temp < 0)
        {
            // we will get here again when we dont have anything to send in that case we chack if errno is eagain and then modity fd to EPOLLOUT so it is ready to take input again. in that case return true.
            if (errno == EAGAIN)
            {
                modfd(m_epollfd, req.m_sockfd, EPOLLOUT, m_trigger_mode);
                return true;
            }
            res.unmap();
            return false;
        }
        res.bytes_have_send += temp;
        res.bytes_to_send -= temp;
        if (res.bytes_have_send >= res.m_iv[0].iov_len) // if response header have been sent
        {
            res.m_iv[0].iov_len = 0; // make first buffer zero
            // give the 2nd buufer of file stored in memory map
            res.m_iv[1].iov_base = res.m_file_address + (res.bytes_have_send - res.m_write_idx);
            res.m_iv[1].iov_len = res.bytes_to_send;
        }
        else
        {
            res.m_iv[0].iov_base = res.m_write_buf + res.bytes_have_send;    // move pointer to rest have header that is remaining
            res.m_iv[0].iov_len = res.m_iv[0].iov_len - res.bytes_have_send; // remaining header data
        }
        if (res.bytes_to_send <= 0) // we have sent everything to client
        {
            res.unmap();
            modfd(m_epollfd, req.m_sockfd, EPOLLIN, m_trigger_mode);
            if (req.m_linger) // if connection is keep alive
            {
                init(); // reintialize all variable and buffers;
                return true;
            }
            else
                return false;
        }
    }
}

void HTTP_CONN::process()
{
    HTTP_CODE read_ret = process_read(); // read from buffer
    if (read_ret == NO_REQUEST)          // if we dont have any request or bad request
    {
        // EPOLLIN file descriptor is ready for read operation for response
        modfd(m_epollfd, req.m_sockfd, EPOLLIN, m_trigger_mode);
        return;
    }

    router.handleRequest(req, res);

    // if everything is ready tell the evernt loop to write to the socket through epoll
    modfd(m_epollfd, req.m_sockfd, EPOLLOUT, m_trigger_mode);
}