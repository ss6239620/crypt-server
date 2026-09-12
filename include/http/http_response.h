#ifndef HTTP_RESPONSE_H
#define HTTP_RESPONSE_H

#include <cstddef>
#include <map>
#include <string>
#include <sys/stat.h>
#include <sys/uio.h>

/**
 * @class HttpResponse
 * @brief Builds a complete HTTP response and exposes writev-ready buffers.
 *
 * Dynamic responses use owned std::string buffers. Static files keep the body
 * in an mmap() region so large files do not need to be copied into memory.
 */
class HttpResponse
{
public:
    HttpResponse();
    ~HttpResponse();

    /**
     * @brief Clear the current response before building another one.
     */
    void reset();

    /**
     * @brief Set the static file root used by render().
     */
    void set_doc_root(const std::string &root);

    /**
     * @brief Copy the request keep-alive decision into the response.
     */
    void set_keep_alive(bool keep_alive);

    /**
     * @brief Add or replace a header that should be emitted with the response.
     */
    void set_header(const std::string &name, const std::string &value);

    /**
     * @brief Send a text response from a route handler.
     */
    bool send(int status, const std::string &content);

    /**
     * @brief Send a dynamic response with an explicit MIME type.
     */
    bool send(int status, const std::string &content, const std::string &content_type);

    /**
     * @brief Send a file under doc_root using mmap() for the body.
     */
    bool render(int status, const std::string &file_name);

    /**
     * @brief Release the mapped file body if this response owns one.
     */
    void unmap();

    /**
     * @brief true after send() or render() has built bytes for the socket.
     */
    bool ready() const;

    /**
     * @brief Total bytes left to send from header + body/file.
     */
    size_t total_bytes() const;

    /**
     * @brief Fill iovec entries for writev() starting at a byte offset.
     * @return Number of iovec entries filled: 0, 1, or 2.
     */
    int build_iovecs(size_t write_offset, struct iovec out[2]) const;

    /**
     * @brief Values are useful later for the route cache layer.
     */
    int status_code() const;
    const std::string &body() const;
    const std::string &content_type() const;

private:
    std::string doc_root_;                ///< Absolute static file root.
    std::string header_buffer_;           ///< Fully rendered HTTP status line + headers.
    std::string body_buffer_;             ///< Owned response body for dynamic send().
    std::string content_type_;            ///< Last response MIME type.
    std::map<std::string, std::string> headers_; ///< Custom and automatic response headers.
    std::string mapped_file_path_;        ///< Path used for logging/debugging mapped files.
    char *file_address_;                  ///< mmap() body pointer for static files.
    size_t file_size_;                    ///< Number of bytes in the mapped file body.
    bool file_response_;                  ///< true means body bytes come from file_address_.
    bool response_ready_;                 ///< true means write() can send this response.
    bool keep_alive_;                     ///< true writes "Connection: keep-alive"; false writes "Connection: close".
    int status_code_;                     ///< Last response status.
    struct stat file_stat_;               ///< stat() result for the mapped file.

    bool map_file(const std::string &file_name);
    bool unsafe_path(const std::string &file_name) const;
    void build_headers(size_t content_length);
    const char *status_message(int status) const;
    std::string content_type_for_path(const std::string &file_name) const;
};

#endif
