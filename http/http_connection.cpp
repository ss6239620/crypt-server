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

const char *ok_200_tite = "OK";
const char *error_400_tite = "Bad Request";
const char *error_400_form = "Your requets have bad syntax or is inhreently impossible to satisfy.\n";
const char *error_403_tite = "Forbidden";
const char *error_403_form = "You do not have permission to get file from this server.\n";
const char *error_404_tite = "Not Found";
const char *error_404_form = "The request file was not found on this server.\n";
const char *error_500_tite = "Internal Error";
const char *error_500_form = "There was an unusual problem solving the request file.\n";

LOCKER m_lock;             // lock for shared resource
map<string, string> users; // username password hasmap for fast lookup

void HTTP_CONN::initmysql_result(DB_CONNECTION_POOL *conn_pool)
{
    MYSQL *mysql = NULL;
    CONNECTION_POOL_RAII mysqlcon(&mysql, conn_pool);

    if (mysql_query(mysql, "SELECT username,passwd FROM user"))
    {
        LOG_ERROR("SELECT error: %s\n", mysql_error(mysql));
    }
    MYSQL_RES *result = mysql_store_result(mysql);

    int num_field = mysql_num_fields(result);

    MYSQL_FIELD *fields = mysql_fetch_field(result);

    while (MYSQL_ROW row = mysql_fetch_row(result))
    {
        string temp1(row[0]);
        string temp2(row[1]);
        users[temp1] = temp2;
    }
}

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
    if (real_close && (m_sockfd != -1)) // if sockfd is -1 than it that socket is already closed.
    {
        printf("close %d\n", m_sockfd);
        removefd(m_epollfd, m_sockfd);
        m_sockfd = -1;
        m_user_count--;
    }
}

void HTTP_CONN::init(int sockfd, const sockaddr_in &addr, char *root, int trigger_mode, int close_log, string user, string password, string sqlname)
{
    m_sockfd = sockfd;
    m_address = addr;

    addfd(m_epollfd, sockfd, true, trigger_mode);
    m_user_count++;

    doc_root = root;
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
    bytes_to_send = 0;
    bytes_have_send = 0;
    m_check_state = CHECK_STATE_REQUESTLINE;
    m_linger = false;
    m_method = GET;
    m_url = 0;
    m_version = 0;
    m_content_length = 0;
    m_host = 0;
    m_start_line = 0;
    m_checked_idx = 0;
    m_read_idx = 0;
    m_write_idx = 0;
    cgi = 0;
    m_state = 0;
    timer_flag = 0;
    improv = 0;

    memset(m_read_buf, '\0', READ_BUFFER_SIZE);
    memset(m_write_buf, '\0', WRITE_BUFFER_SIZE);
    memset(m_real_file, '\0', FILENAME_LEN);
}

bool HTTP_CONN::read_once()
{
    if (m_read_idx >= READ_BUFFER_SIZE)
        return false;

    int byte_read = 0;

    /*
      In LT mode, if there’s still data left, the event remains active in epoll and will be triggered again.
      The function doesn't need a loop because epoll_wait() will notify again if more data is available.
     */
    if (0 == m_trigger_mode)
    {
        byte_read = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);
        m_read_idx += byte_read;

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
            byte_read = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);

            if (byte_read == -1) // recv() error
            {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                    break; // No more data left to read -> exit loop

                return false; // Some other error occurred, close connection
            }
            // if bytes_read is 0 we have succesfully read the data in buffer hence we return and loop automatically break.
            else if (byte_read == 0) // Client closed the connection byte read is 0 when cliend closed connection
                return false;

            m_read_idx += byte_read;
        }
        return true; // Successfully read all available data
    }
}

HTTP_CONN::LINE_STATUS HTTP_CONN::parse_line()
{
    char temp;
    for (; m_checked_idx < m_read_idx; ++m_checked_idx)
    {
        temp = m_read_buf[m_checked_idx]; // read a charactor from buffer.
        if (temp == '\r')                 // we have reached end of curr line
        {
            // If '\r' is the last character read, we need more data
            if ((m_checked_idx + 1) == m_read_idx)
                return LINE_OPEN;
            // If the next charactor is new_line than we have reached end of current line we make both charctor as '\0'
            else if (m_read_buf[m_checked_idx + 1] == '\n')
            {
                m_read_buf[m_checked_idx++] = '\0';
                m_read_buf[m_checked_idx++] = '\0'; // move to the first charctor of next line.
                return LINE_OK;
            }
            return LINE_BAD;
        }
        // genearlly this case is not required as most system can uses '\r' but some legacy system use only '\n' instead of \r\n or some misformed request so we handle it using this
        else if (temp == '\n')
        {
            // chack if revious charctor is \r if it is replace both \r\n with \0\0
            if (m_checked_idx > 1 && m_read_buf[m_checked_idx - 1] == '\r')
            {
                m_read_buf[m_checked_idx - 1] = '\0';
                m_read_buf[m_checked_idx++] = '\0'; // move to the first charctor of next line.
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    return LINE_OPEN;
}

HTTP_CONN::HTTP_CODE HTTP_CONN::parse_request_line(char *text) // helps in parsing 1st request line
{
    m_url = strpbrk(text, " \t"); // Find the first space or tab
    if (!m_url)
        return BAD_REQUEST;
    *m_url++ = '\0'; // replace space with \0
    // text="GET\0/index.html HTTP/1.1\r\n and m_url="  /index.html HTTP/1.1\r\n"

    char *method = text;
    if (strcasecmp(method, "GET") == 0) // match found as "GET\0"
        m_method = GET;
    else if (strcasecmp(method, "POST") == 0)
    {
        m_method = POST;
        cgi = 1; // enable CGI
    }
    else
        return BAD_REQUEST;

    m_url += strspn(m_url, " \t"); // This line is used to skip leading spaces
    // m_url = "/index.html HTTP/1.1\r\n"

    m_version = strpbrk(m_url, " \t");
    // m_version = "  HTTP/1.1\r\n"

    if (!m_version)
        return BAD_REQUEST;
    *m_version++ = '\0';
    // m_url: "/index.html\0HTTP/1.1\r\n"

    m_version += strspn(m_version, " \t");
    // m_version = "HTTP/1.1\r\n"
    if (strcasecmp(m_version, "HTTP/1.1") != 0) // match found
        return BAD_REQUEST;

    // Handle URL Starting with http:// or https:// we make it jump to 7 or 8
    if (strncasecmp(m_url, "http://", 7) == 0)
    {
        m_url += 7;
        m_url = strchr(m_url, '/');
    }
    if (strncasecmp(m_url, "https://", 8) == 0)
    {
        m_url += 8;
        m_url = strchr(m_url, '/');
    }
    // check if m_url has / or not
    if (!m_url || m_url[0] != '/')
        return BAD_REQUEST;
    if (strlen(m_url) == 1)
        strcat(m_url, "judge.html");
    m_check_state = CHECK_STATE_HEADER;

    return NO_REQUEST;

    /*here is something intresting m_url at last will be this /index.html\0HTTP/1.1\r\n but it will be stored as /index.html\0 because of \0 but what about m_version=HTTP/1.1\r\n it still contain \r\n it will be processed by parse_line function.
     */
}

HTTP_CONN::HTTP_CODE HTTP_CONN::parse_headers(char *text)
{
    if (text[0] == '\0') // we have reached end of header as we encounterd space line
    {
        if (m_content_length != 0) // when we have some content
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
            m_linger = true;
    }
    else if (strncasecmp(text, "Content-length:", 15) == 0)
    {
        text += 15;
        text += strspn(text, " \t"); // skip leading zero
        m_content_length = atoi(text);
    }
    else if (strncasecmp(text, "Host:", 5) == 0)
    {
        text += 5;
        text += strspn(text, " \t");
        m_host = text;
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
    if (m_read_idx >= (m_content_length + m_checked_idx))
    {

        text[m_content_length] = '\0'; // put \0 at end of body
        m_string = text;
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
        m_start_line = m_checked_idx;
        LOG_INFO("%s", text);
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
                return do_request(); // if it is get request fo request
            break;
        }
        case CHECK_STATE_CONTENT:
        {
            ret = parse_content(text);
            if (ret == GET_REQUEST)
                return do_request(); // after parsing content make request
            line_status = LINE_OPEN;
            break;
        }
        default:
            return INTERNAL_ERROR;
        }
    }
    return NO_REQUEST;
}

HTTP_CONN::HTTP_CODE HTTP_CONN::do_request()
{
    strcpy(m_real_file, doc_root); // copy name of folder in filename m_real_file has full of \0
    int len = strlen(doc_root);
    const char *p = strrchr(m_url, '/'); // find last occurance of / in m_url

    // this checks cgi=1 and p+1 is 2 or 3
    if (cgi == 1 && (*(p + 1) == '2' || *(p + 1) == '3'))
    {
        // Determine whether it is login detection or registration detection based on the flag
        char flag = m_url[1];

        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/");       // add "/"
        strcat(m_url_real, m_url + 2); // skip / and number ans append rest of string
        // append m_url_real name in real_file buffer
        strncpy(m_real_file + len, m_url_real, FILENAME_LEN - len - 1);
        free(m_url_real);

        // for now we will be sending fixed post data as
        // user=123&passwd=123
        char name[100], passwd[100];

        int i;
        // save user name first as "user="5 charactor we replace until we encounter &
        for (i = 5; m_string[i] != '&'; ++i)
            name[i - 5] = m_string[i];

        name[i - 5] = '\0'; // make & as \0

        int j = 0;
        // save user passwd first as "passwd=123"10 charactor we replace until we encounter &
        for (i = i + 10; m_string[i] != '\0'; ++i, ++j)
            passwd[j] = m_string[i];
        passwd[j] = '\0';

        if (*(p + 1) == '3') // if registration
        {
            char *sql_insert = (char *)malloc(sizeof(char) * 200);
            strcpy(sql_insert, "INSERT INTO user(username,passwd) VALUES(");
            strcat(sql_insert, "'");
            strcat(sql_insert, name);
            strcat(sql_insert, "', '");
            strcat(sql_insert, passwd);
            strcat(sql_insert, "')");

            if (users.find(name) == users.end()) // not in db
            {
                m_lock.lock();
                int res = mysql_query(mysql, sql_insert);
                users.insert(pair<string, string>(name, passwd));
                m_lock.unlock();

                if (!res)
                    strcpy(m_url, "/log.html");
                else
                    strcpy(m_url, "/registerError.html");
            }
            else
                strcpy(m_url, "/registerError.html");
        }
        else if (*(p + 1) == '2') // if login
        {
            if (users.find(name) != users.end() && users[name] == passwd)
                strcpy(m_url, "/welcome.html");
            else
                strcpy(m_url, "/logError.html");
        }
    }
    if (*(p + 1) == '0') // send register.html file
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/register.html");                       // copy file name
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real)); // attach file name to doc_root
        free(m_url_real);
    }
    else if (*(p + 1) == '1') // send log.html file
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/log.html");                            // copy file name
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real)); // attach file name to doc_root
        free(m_url_real);
    }
    else if (*(p + 1) == '5') // send picture.html file
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/picture.html");                        // copy file name
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real)); // attach file name to doc_root
        free(m_url_real);
    }
    else if (*(p + 1) == '6') // send video.html file
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/video.html");                          // copy file name
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real)); // attach file name to doc_root
        free(m_url_real);
    }
    else if (*(p + 1) == '7') // send fans.html file
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/fans.html");                           // copy file name
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real)); // attach file name to doc_root
        free(m_url_real);
    }
    else
        strncpy(m_real_file + len, m_url, FILENAME_LEN - len - 1); // attach same url and send it back

    // check file stat and put in file_stat less than 0 no resource.
    if (stat(m_real_file, &m_file_stat) < 0)
        return NO_RESOURCE;

    // if user dosent have read permission over file send forbidden request
    if (!(m_file_stat.st_mode & S_IROTH))
        return FORBIDDEN_REQUEST;

    // if request file is directory send bad request
    if (S_ISDIR(m_file_stat.st_mode))
        return BAD_REQUEST;

    // if everything is succesfull open the file in mapped memory space for faster access using mmap
    int fd = open(m_real_file, O_RDONLY); // open in read only mode
    /**
     * here we are opening the file in system memory for faster access and storing the address of that space in m_file_address variable the paramater used are as follow:
     * 1) 0 - let system decide starting address of the file
     * 2) size of the file
     * 3) PROT_READ open only in reading mode we cannot make any changes to memory ther by file
     * 4) MAP_PRIVATE here if it is private it does not coomit changes back to file
     * 5) file descriptor
     */
    m_file_address = (char *)mmap(0, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    return FILE_REQUEST;
}

void HTTP_CONN::unmap()
{
    if (m_file_address)
    {
        munmap(m_file_address, m_file_stat.st_size);
        m_file_address = 0;
    }
}

bool HTTP_CONN::write()
{
    int temp = 0;
    if (bytes_to_send == 0)
    {
        // EPOLLIN file descriptor is ready for read operation for response
        modfd(m_epollfd, m_sockfd, EPOLLIN, m_trigger_mode); // modify fs to listen again for output
        init();                                              // reintialize everything
        return true;
    }

    while (1)
    {
        // with writev we can write on multiple buffer it returns number of bytes written on error -1
        temp = writev(m_sockfd, m_iv, m_iv_count);
        if (temp < 0)
        {
            // we will get here again when we dont have anything to send in that case we chack if errno is eagain and then modity fd to EPOLLOUT so it is ready to take input again. in that case return true.
            if (errno == EAGAIN)
            {
                modfd(m_epollfd, m_sockfd, EPOLLOUT, m_trigger_mode);
                return true;
            }
            unmap();
            return false;
        }
        bytes_have_send += temp;
        bytes_to_send -= temp;
        if (bytes_have_send >= m_iv[0].iov_len) // if response header have been sent
        {
            m_iv[0].iov_len = 0; // make first buffer zero
            // give the 2nd buufer of file stored in memory map
            m_iv[1].iov_base = m_file_address + (bytes_have_send - m_write_idx);
            m_iv[1].iov_len = bytes_to_send;
        }
        else
        {
            m_iv[0].iov_base = m_write_buf + bytes_have_send;    // move pointer to rest have header that is remaining
            m_iv[0].iov_len = m_iv[0].iov_len - bytes_have_send; // remaining header data
        }
        if (bytes_to_send <= 0) // we have sent everything to client
        {
            unmap();
            modfd(m_epollfd, m_sockfd, EPOLLIN, m_trigger_mode);
            if (m_linger) // if connection is keep alive
            {
                init(); // reintialize all variable and buffers;
                return true;
            }
            else
                return false;
        }
    }
}

bool HTTP_CONN::add_response(const char *format, ...)
{
    if (m_write_idx >= WRITE_BUFFER_SIZE) // if buffer is full
        return false;

    // make a list of argument and put all argument in arg_llist after format
    va_list arg_list;
    va_start(arg_list, format);

    /*write variable formated from multiple arguments here we will move across write buffer using m_write_idx and write formated input in buffer and it returns number of char written
     */
    int len = vsnprintf(m_write_buf + m_write_idx, WRITE_BUFFER_SIZE - 1 - m_write_idx, format, arg_list);

    // if buffer isnt sufficent enough clear arg_list and end it.
    if (len >= (WRITE_BUFFER_SIZE - 1 - m_write_idx))
    {
        va_end(arg_list);
        return false;
    }
    m_write_idx += len; // move m_write_idx by len
    va_end(arg_list);

    LOG_INFO("request: %s", m_write_buf);
    return true;
}

bool HTTP_CONN::add_status_line(int status, const char *title)
{
    return add_response("%s %d %s\r\n", "HTTP/1.1", status, title);
}

bool HTTP_CONN::add_headers(int content_length)
{
    return add_content_length(content_length) && add_linger() && add_blank_line();
}

bool HTTP_CONN::add_content_length(int content_length)
{
    return add_response("Content-Length:%d\r\n", content_length);
}

bool HTTP_CONN::add_content_type()
{
    return add_response("Content-Type:%s\r\n", "text/html");
}

bool HTTP_CONN::add_linger()
{
    return add_response("Connection:%s\r\n", (m_linger == true) ? "keep-alive" : "close");
}

bool HTTP_CONN::add_blank_line()
{
    return add_response("%s", "\r\n");
}

bool HTTP_CONN::add_content(const char *content)
{
    return add_response("%s", content);
}

bool HTTP_CONN::process_write(HTTP_CODE ret)
{
    switch (ret)
    {
    case INTERNAL_ERROR:
    {
        add_status_line(500, error_500_tite);
        add_headers(strlen(error_500_form));
        if (!add_content(error_500_form))
            return false;
        break;
    }
    case BAD_REQUEST:
    {
        add_status_line(404, error_404_tite);
        add_headers(strlen(error_404_form));
        if (!add_content(error_404_form))
            return false;
        break;
    }
    case FORBIDDEN_REQUEST:
    {
        add_status_line(403, error_403_tite);
        add_headers(strlen(error_403_form));
        if (!add_content(error_403_form))
            return false;
        break;
    }
    case FILE_REQUEST:
    {
        add_status_line(200, ok_200_tite);
        if (m_file_stat.st_size != 0) // if file size in not zero
        {
            add_headers(m_file_stat.st_size); // file size
            // m_iv[0] contain header information
            m_iv[0].iov_base = m_write_buf; // write buffer
            m_iv[0].iov_len = m_write_idx;  // length of data to send from buffer
            // m_iv[1] contain file data
            m_iv[1].iov_base = m_file_address;                 // mapped memmory address
            m_iv[1].iov_len = m_file_stat.st_size;             // size of file
            m_iv_count = 2;                                    // we have 2 buffer
            bytes_to_send = m_write_idx + m_file_stat.st_size; // total data to send
            return true;
        }
        else
        {
            const char *ok_sting = "<html><body></body></html>";
            add_headers(strlen(ok_sting));
            if (!add_content(ok_sting))
                return false;
        }
    }
    default:
        return false;
    }
    // m_iv[0] contain header information
    m_iv[0].iov_base = m_write_buf; // header buffer
    m_iv[0].iov_len = m_write_idx;  // length of data to send from buffer
    m_iv_count = 1;
    bytes_to_send = m_write_idx; // we only have to send header
    return true;
}

void HTTP_CONN::process()
{
    HTTP_CODE read_ret = process_read(); // read from buffer
    if (read_ret == NO_REQUEST)          // if we dont have any request or bad request
    {
        // EPOLLIN file descriptor is ready for read operation for response
        modfd(m_epollfd, m_sockfd, EPOLLIN, m_trigger_mode);
        return;
    }
    bool write_ret = process_write(read_ret); // make everything ready for write
    if (!write_ret)
        close_conn();
    // if everything is ready tell the evernt loop to write to the socket through epoll
    modfd(m_epollfd, m_sockfd, EPOLLOUT, m_trigger_mode);
}