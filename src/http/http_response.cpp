#include "http_response.h"

#include "../log/log.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

namespace
{
    std::string lowercase_copy(const std::string &value)
    {
        std::string lowered = value;
        std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                       [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        return lowered;
    }
}

HttpResponse::HttpResponse()
    : file_address_(nullptr),
      file_size_(0),
      file_response_(false),
      response_ready_(false),
      keep_alive_(false),
      status_code_(200)
{
    std::memset(&file_stat_, 0, sizeof(file_stat_));
}

HttpResponse::~HttpResponse()
{
    unmap();
}

void HttpResponse::reset()
{
    unmap();
    header_buffer_.clear();
    body_buffer_.clear();
    content_type_.clear();
    headers_.clear();
    mapped_file_path_.clear();
    file_size_ = 0;
    file_response_ = false;
    response_ready_ = false;
    keep_alive_ = false;
    status_code_ = 200;
}

void HttpResponse::set_doc_root(const std::string &root)
{
    doc_root_ = root;
}

void HttpResponse::set_keep_alive(bool keep_alive)
{
    keep_alive_ = keep_alive;
}

void HttpResponse::set_header(const std::string &name, const std::string &value)
{
    headers_[name] = value;
}

bool HttpResponse::send(int status, const std::string &content)
{
    return send(status, content, "text/plain");
}

bool HttpResponse::send(int status, const std::string &content, const std::string &content_type)
{
    unmap(); // Dynamic responses own their body string, so any old mapped file must be released.
    header_buffer_.clear();
    body_buffer_ = content;
    mapped_file_path_.clear();
    file_size_ = 0;
    file_response_ = false;
    status_code_ = status;
    content_type_ = content_type;

    build_headers(body_buffer_.size());
    response_ready_ = true;

    LOG_INFO("Response status=%d bytes=%zu", status_code_, total_bytes());
    return true;
}

bool HttpResponse::render(int status, const std::string &file_name)
{
    unmap(); // A new static response should not leave the previous mmap() region alive.
    header_buffer_.clear();
    body_buffer_.clear();
    file_size_ = 0;
    file_response_ = false;
    response_ready_ = false;
    status_code_ = status;
    content_type_ = content_type_for_path(file_name);

    if (!map_file(file_name))
        return false;

    build_headers(file_size_);
    file_response_ = true;
    response_ready_ = true;

    LOG_INFO("Static file response path=%s bytes=%zu", mapped_file_path_.c_str(), total_bytes());
    return true;
}

void HttpResponse::unmap()
{
    if (file_address_ != nullptr)
    {
        // mmap() memory is owned by the response and must be returned to the OS after writev() finishes.
        munmap(file_address_, file_size_);
        file_address_ = nullptr;
    }
}

bool HttpResponse::ready() const
{
    return response_ready_;
}

size_t HttpResponse::total_bytes() const
{
    size_t payload_size = file_response_ ? file_size_ : body_buffer_.size();
    return header_buffer_.size() + payload_size;
}

int HttpResponse::build_iovecs(size_t write_offset, struct iovec out[2]) const
{
    size_t header_size = header_buffer_.size();
    const char *payload_data = file_response_ ? file_address_ : body_buffer_.data();
    size_t payload_size = file_response_ ? file_size_ : body_buffer_.size();

    if (write_offset >= header_size + payload_size)
        return 0;

    if (write_offset < header_size)
    {
        // First iovec points into the remaining header bytes.
        out[0].iov_base = const_cast<char *>(header_buffer_.data() + write_offset);
        out[0].iov_len = header_size - write_offset;

        if (payload_size == 0)
            return 1;

        // Second iovec points at the body/file, so writev() can send both with one syscall.
        out[1].iov_base = const_cast<char *>(payload_data);
        out[1].iov_len = payload_size;
        return 2;
    }

    size_t payload_offset = write_offset - header_size;
    if (payload_offset >= payload_size)
        return 0;

    out[0].iov_base = const_cast<char *>(payload_data + payload_offset);
    out[0].iov_len = payload_size - payload_offset;
    return 1;
}

int HttpResponse::status_code() const
{
    return status_code_;
}

const std::string &HttpResponse::body() const
{
    return body_buffer_;
}

const std::string &HttpResponse::content_type() const
{
    return content_type_;
}

bool HttpResponse::map_file(const std::string &file_name)
{
    if (doc_root_.empty())
        return false;

    if (unsafe_path(file_name))
        return false;

    std::string safe_name = file_name.empty() ? "/judge.html" : file_name;
    if (safe_name[0] != '/')
        safe_name.insert(safe_name.begin(), '/');

    mapped_file_path_ = doc_root_ + safe_name;

    if (stat(mapped_file_path_.c_str(), &file_stat_) < 0)
    {
        LOG_ERROR("File not found: %s", mapped_file_path_.c_str());
        return false;
    }

    if (!(file_stat_.st_mode & S_IROTH))
    {
        LOG_ERROR("Insufficient permissions for file: %s", mapped_file_path_.c_str());
        return false;
    }

    if (S_ISDIR(file_stat_.st_mode))
    {
        LOG_ERROR("Requested path is a directory: %s", mapped_file_path_.c_str());
        return false;
    }

    file_size_ = static_cast<size_t>(file_stat_.st_size);
    if (file_size_ == 0)
        return true;

    int fd = open(mapped_file_path_.c_str(), O_RDONLY);
    if (fd < 0)
    {
        LOG_ERROR("Failed to open file: %s", mapped_file_path_.c_str());
        return false;
    }

    file_address_ = static_cast<char *>(mmap(nullptr, file_size_, PROT_READ, MAP_PRIVATE, fd, 0));
    close(fd);

    if (file_address_ == MAP_FAILED)
    {
        LOG_ERROR("Failed to mmap file: %s", mapped_file_path_.c_str());
        file_address_ = nullptr;
        file_size_ = 0;
        return false;
    }

    return true;
}

bool HttpResponse::unsafe_path(const std::string &file_name) const
{
    // The server serves only files below doc_root_; ".." would allow escaping that directory.
    return file_name.find("..") != std::string::npos;
}

void HttpResponse::build_headers(size_t content_length)
{
    headers_["Content-Length"] = std::to_string(content_length);
    headers_["Content-Type"] = content_type_.empty() ? "text/plain" : content_type_;
    headers_["Connection"] = keep_alive_ ? "keep-alive" : "close";

    header_buffer_ = "HTTP/1.1 ";
    header_buffer_ += std::to_string(status_code_);
    header_buffer_ += " ";
    header_buffer_ += status_message(status_code_);
    header_buffer_ += "\r\n";

    for (const auto &header : headers_)
    {
        header_buffer_ += header.first;
        header_buffer_ += ": ";
        header_buffer_ += header.second;
        header_buffer_ += "\r\n";
    }

    // The empty line tells the HTTP client that headers ended and the body starts next.
    header_buffer_ += "\r\n";
}

const char *HttpResponse::status_message(int status) const
{
    switch (status)
    {
    case 200:
        return "OK";
    case 400:
        return "Bad Request";
    case 404:
        return "Not Found";
    case 405:
        return "Method Not Allowed";
    case 413:
        return "Payload Too Large";
    case 500:
        return "Internal Server Error";
    case 503:
        return "Service Unavailable";
    default:
        return "Unknown Status";
    }
}

std::string HttpResponse::content_type_for_path(const std::string &file_name) const
{
    size_t dot = file_name.find_last_of('.');
    if (dot == std::string::npos)
        return "application/octet-stream";

    std::string ext = lowercase_copy(file_name.substr(dot + 1));
    if (ext == "html" || ext == "htm")
        return "text/html";
    if (ext == "css")
        return "text/css";
    if (ext == "js")
        return "application/javascript";
    if (ext == "json")
        return "application/json";
    if (ext == "png")
        return "image/png";
    if (ext == "jpg" || ext == "jpeg")
        return "image/jpeg";
    if (ext == "gif")
        return "image/gif";
    if (ext == "ico")
        return "image/x-icon";
    if (ext == "mp4")
        return "video/mp4";

    return "application/octet-stream";
}
