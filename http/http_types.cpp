#include "http_types.h"
#include <cstdarg>
#include <cstdio>

bool HttpResponse::add_response(const char *format, ...)
{
    if (m_write_idx >= WRITE_BUFFER_SIZE)
        return false;

    va_list arg_list;
    va_start(arg_list, format);

    int len = vsnprintf(m_write_buf + m_write_idx,
                        WRITE_BUFFER_SIZE - 1 - m_write_idx,
                        format,
                        arg_list);

    if (len >= (WRITE_BUFFER_SIZE - 1 - m_write_idx))
    {
        va_end(arg_list);
        return false;
    }
    m_write_idx += len;
    va_end(arg_list);

    LOG_INFO("request: %s", m_write_buf);
    return true;
}

bool HttpResponse::add_status_line(int status, const char *title)
{
    return add_response("%s %d %s\r\n", "HTTP/1.1", status, title);
}

bool HttpResponse::add_headers(int content_length)
{
    return add_content_length(content_length) &&
           add_linger() &&
           add_blank_line();
}

bool HttpResponse::add_content_length(int content_length)
{
    return add_response("Content-Length:%d\r\n", content_length);
}

bool HttpResponse::add_content_type(const char *type)
{
    return add_response("Content-Type:%s\r\n", type);
}

bool HttpResponse::add_linger()
{
    return add_response("Connection:%s\r\n", m_linger ? "keep-alive" : "close");
}

bool HttpResponse::add_blank_line()
{
    return add_response("%s", "\r\n");
}

bool HttpResponse::add_content(const char *content)
{
    return add_response("%s", content);
}

void HttpResponse::unmap()
{
    if (m_file_address)
    {
        munmap(m_file_address, m_file_stat.st_size);
        m_file_address = 0;
    }
}

bool HttpResponse::send(int status, const std::string &content)
{
    const char *content_type = "text/plain";

    // Reset write buffer and response state
    m_write_idx = 0;
    bytes_to_send = 0;
    bytes_have_send = 0;
    m_iv_count = 0;

    // Build the response
    if (!add_status_line(status, get_status_message(status)))
        return false;

    if (!add_content_type(content_type))
        return false;

    if (!add_content_length(content.length()))
        return false;

    if (!add_linger())
        return false;

    if (!add_blank_line())
        return false;

    if (!add_content(content.c_str()))
        return false;

    // Set up the I/O vector for writev
    m_iv[0].iov_base = m_write_buf;
    m_iv[0].iov_len = m_write_idx;
    m_iv_count = 1;
    bytes_to_send = m_write_idx;
    // Log the response
    LOG_INFO("Response: %.*s", m_write_idx, m_write_buf);

    return true;
}

bool HttpResponse::mapfile(const char *file_name)
{
    // Reset write buffer and response state
    m_write_idx = 0;
    bytes_to_send = 0;
    bytes_have_send = 0;
    m_iv_count = 0;

    strcpy(m_real_file, doc_root); // copy name of folder in filename m_real_file has full of \0
    int len = strlen(doc_root);

    // Construct full file path
    char *m_url_real = (char *)malloc(sizeof(char) * 200);
    if (!m_url_real)
        return false;

    strcpy(m_url_real, file_name);
    strncpy(m_real_file + len, m_url_real, strlen(m_url_real));
    free(m_url_real);

    // Check file existence and permissions
    if (stat(m_real_file, &m_file_stat) < 0)
    {
        LOG_ERROR("File not found: %s", m_real_file);
        return false;
    }

    if (!(m_file_stat.st_mode & S_IROTH))
    {
        LOG_ERROR("Insufficient permissions for file: %s", m_real_file);
        return false;
    }

    if (S_ISDIR(m_file_stat.st_mode))
    {
        LOG_ERROR("Requested path is a directory: %s", m_real_file);
        return false;
    }

    // Map file to memory
    int fd = open(m_real_file, O_RDONLY);
    if (fd < 0)
    {
        LOG_ERROR("Failed to open file: %s", m_real_file);
        return false;
    }

    m_file_address = (char *)mmap(0, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);

    if (m_file_address == MAP_FAILED)
    {
        LOG_ERROR("Failed to mmap file: %s", m_real_file);
        m_file_address = 0;
        return false;
    }

    return true;
}

bool HttpResponse::render(int status, const std::string &file_name)
{
    // Reset response state
    m_write_idx = 0;
    bytes_to_send = 0;
    bytes_have_send = 0;
    m_iv_count = 0;

    // Determine content type based on file extension
    const char *content_type = "text/html";
    std::string ext = file_name.substr(file_name.find_last_of('.') + 1);
    if (ext == "css")
        content_type = "text/css";
    else if (ext == "js")
        content_type = "application/javascript";
    else if (ext == "png")
        content_type = "image/png";
    else if (ext == "jpg" || ext == "jpeg")
        content_type = "image/jpeg";
    else if (ext == "gif")
        content_type = "image/gif";
    else if (ext == "ico")
        content_type = "image/x-icon";
    else if (ext == "mp4")
        content_type = "video/mp4";

    // Map the file to memory
    if (!mapfile(file_name.c_str()))
    {
        LOG_ERROR("Failed to map file: %s", file_name.c_str());
        unmap();
        return false;
    }

    // Build response headers
    if (!add_status_line(status, get_status_message(status)) ||
        !add_content_type(content_type) ||
        !add_content_length(m_file_stat.st_size) ||
        !add_linger() ||
        !add_blank_line())
    {
        unmap();
        return false;
    }

    // Set up I/O vectors
    m_iv[0].iov_base = m_write_buf;
    m_iv[0].iov_len = m_write_idx;
    m_iv[1].iov_base = m_file_address;
    m_iv[1].iov_len = m_file_stat.st_size;
    m_iv_count = 2;
    bytes_to_send = m_write_idx + m_file_stat.st_size;

    cout<<"files send "<<file_name<<endl;
 
    return true;
}