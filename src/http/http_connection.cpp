/**
 * Edge-Triggered (ET) Mode DESC:
 * - Events are triggered only when the state of the file descriptor changes.
 * - If you do not read all available data, epoll may not notify you again until new data arrives.
 * - This is why ET sockets are drained in a loop until recv() returns EAGAIN/EWOULDBLOCK.
 *
 * Level-Triggered (LT) Mode DESC:
 * - Events are triggered as long as the file descriptor is still readable/writable.
 * - It is safe to read once and let epoll notify again if more bytes remain.
 *
 * IOVEC / writev:
 * - writev() sends multiple buffers with one syscall.
 * - Here the first buffer is usually HTTP headers and the second buffer is body/file bytes.
 * - After a partial write we rebuild iovec from m_write_offset, instead of mutating old pointers.
 */

#include "http/http_connection.h"
#include "http/http_socket_utils.h"

#include <cerrno>
#include <cstring>
#include <sys/uio.h>

int HTTP_CONN::m_user_count = 0;
int HTTP_CONN::m_epollfd = -1;

HTTP_CONN::HTTP_CONN()
    : mysql(NULL),
      m_state(0),
      timer_flag(0),
      improv(0),
      m_trigger_mode(0),
      m_sockfd(-1),
      m_write_offset(0),
      m_read_error(NO_REQUEST),
      router(ROUTER::get_instance())
{
    std::memset(&m_address, 0, sizeof(m_address));
    std::memset(sql_user, 0, sizeof(sql_user));
    std::memset(sql_password, 0, sizeof(sql_password));
    std::memset(sql_name, 0, sizeof(sql_name));
}

HTTP_CONN::~HTTP_CONN()
{
    res.unmap();
}

void HTTP_CONN::close_conn(bool real_close)
{
    if (real_close && m_sockfd != -1) // -1 means this connection object is not attached to a live socket.
    {
        LOG_INFO("close %d\n", m_sockfd);
        removefd(m_epollfd, m_sockfd);
        m_sockfd = -1;
        m_user_count--;
    }
}

void HTTP_CONN::init(int sockfd, const sockaddr_in &addr, int trigger_mode, int close_log, std::string user, std::string password, std::string sqlname)
{
    (void)close_log;

    m_sockfd = sockfd;
    m_address = addr;

    addfd(m_epollfd, sockfd, true, trigger_mode);
    m_user_count++;
    m_trigger_mode = trigger_mode;

    // Copy database credentials once for this connection object.
    strcpy(sql_user, user.c_str());
    strcpy(sql_password, password.c_str());
    strcpy(sql_name, sqlname.c_str());

    init(); // Reset parser/request/response state before the first request.
}

void HTTP_CONN::init()
{
    mysql = NULL;
    m_state = 0;
    timer_flag = 0;
    improv = 0;
    m_read_error = NO_REQUEST;
    m_write_offset = 0;
    m_read_buffer.clear();

    parser.reset(); // A keep-alive connection starts a brand-new HTTP message after each response.
    req.reset();
    res.reset();
    req.client_address = m_address;
    if (router.isStatic())
        res.set_doc_root(router.root_path());
}

bool HTTP_CONN::read_once()
{
    if (m_sockfd == -1)
        return false;

    char buffer[HTTP_READ_CHUNK_SIZE];

    while (true)
    {
        ssize_t bytes_read = recv(m_sockfd, buffer, sizeof(buffer), 0);

        if (bytes_read > 0)
        {
            m_read_buffer.append(buffer, static_cast<size_t>(bytes_read));

            if (m_read_buffer.size() > HTTP_MAX_HEADER_BYTES + HTTP_MAX_BODY_BYTES)
            {
                // Keep the socket alive long enough to send a clean 413 response instead of dropping it silently.
                m_read_error = PAYLOAD_TOO_LARGE;
                return true;
            }

            if (m_trigger_mode == 0)
                return true; // LT mode can read one chunk and wait for epoll to report more data later.

            continue; // ET mode must drain all currently available bytes before returning.
        }

        if (bytes_read == 0)
            return false; // recv() returns 0 when the client closed its side of the connection.

        if (errno == EINTR)
            continue; // A signal interrupted recv(); retry without closing the client.

        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return true; // Non-blocking socket has no more bytes right now.

        return false; // Any other recv() error means this connection cannot be trusted.
    }
}

HTTP_CONN::HTTP_CODE HTTP_CONN::process_read()
{
    if (m_read_error != NO_REQUEST)
        return m_read_error;

    HttpRequestParser::Result result = parser.parse(m_read_buffer, req);

    switch (result)
    {
    case HttpRequestParser::Result::COMPLETE:
        req.client_address = m_address;
        return GET_REQUEST;
    case HttpRequestParser::Result::INCOMPLETE:
        return NO_REQUEST;
    case HttpRequestParser::Result::METHOD_NOT_ALLOWED:
        return METHOD_NOT_ALLOWED;
    case HttpRequestParser::Result::PAYLOAD_TOO_LARGE:
        return PAYLOAD_TOO_LARGE;
    case HttpRequestParser::Result::BAD_REQUEST:
    default:
        return BAD_REQUEST;
    }
}

void HTTP_CONN::make_error_response(HTTP_CODE code)
{
    res.set_keep_alive(false); // Parser errors close the connection so the client cannot reuse bad state.

    switch (code)
    {
    case BAD_REQUEST:
        res.send(400, "400 bad request");
        break;
    case METHOD_NOT_ALLOWED:
        res.set_header("Allow", "GET, POST");
        res.send(405, "405 method not allowed");
        break;
    case PAYLOAD_TOO_LARGE:
        res.send(413, "413 payload too large");
        break;
    default:
        res.send(500, "500 internal server error");
        break;
    }
}

bool HTTP_CONN::write()
{
    if (!res.ready())
    {
        // Nothing was generated; listen for another request instead of spinning on EPOLLOUT.
        init();
        modfd(m_epollfd, m_sockfd, EPOLLIN, m_trigger_mode);
        return true;
    }

    while (true)
    {
        struct iovec iov[2];
        int iov_count = res.build_iovecs(m_write_offset, iov);

        if (iov_count == 0)
            break; // All header/body bytes have already been sent.

        ssize_t bytes_written = writev(m_sockfd, iov, iov_count);

        if (bytes_written < 0)
        {
            if (errno == EINTR)
                continue; // Retry when a signal interrupts writev().

            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                // Socket send buffer is full; wait until epoll reports that it can accept more bytes.
                modfd(m_epollfd, m_sockfd, EPOLLOUT, m_trigger_mode);
                return true;
            }

            res.unmap();
            return false;
        }

        if (bytes_written == 0)
        {
            modfd(m_epollfd, m_sockfd, EPOLLOUT, m_trigger_mode);
            return true;
        }

        m_write_offset += static_cast<size_t>(bytes_written);

        if (m_write_offset >= res.total_bytes())
            break; // The full HTTP response has reached the kernel socket buffer.
    }

    res.unmap();

    if (req.keep_alive)
    {
        init(); // Keep the TCP connection, but clear the HTTP message state for the next request.
        modfd(m_epollfd, m_sockfd, EPOLLIN, m_trigger_mode);
        return true;
    }

    return false; // Returning false lets WEBSERVER remove the timer and close this client fd.
}

void HTTP_CONN::process()
{
    HTTP_CODE read_ret = process_read();

    if (read_ret == NO_REQUEST)
    {
        // The request line/headers/body are incomplete; wait for the next EPOLLIN event.
        modfd(m_epollfd, m_sockfd, EPOLLIN, m_trigger_mode);
        return;
    }

    res.reset();

    if (read_ret == GET_REQUEST)
    {
        res.set_keep_alive(req.keep_alive);
        router.handleRequest(req, res);

        if (!res.ready())
            res.send(500, "500 internal server error");
    }
    else
    {
        make_error_response(read_ret);
    }

    // Response bytes are ready; switch epoll to write mode for this one-shot fd.
    modfd(m_epollfd, m_sockfd, EPOLLOUT, m_trigger_mode);
}
