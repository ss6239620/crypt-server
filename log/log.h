#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <iostream>
#include <string>
#include <cstring>
#include <stdarg.h>
#include <pthread.h>
#include "./block_queue.h"

using namespace std;

/**
 * @class LOG
 * @brief Thread-safe logging system with asynchronous/synchronous modes and log rotation
 */
class LOG
{
private:
    char dir_name[128]; // path name
    char log_name[128]; // log file name
    int m_split_line;   // maximum number of log lines
    int m_log_buf_size; // log buffer size
    long long m_count;  // log line count
    int m_today;        // Because it is calssified by day the current time is recorded on that day
    FILE *m_fp;         // Open the log file pointer
    char *m_buf;
    block_queue<string> *m_log_queue; // Blocking Queue
    bool m_is_async;                  // wherether syncronous or asynchronous
    LOCKER m_mutex;                   // locker
    int m_close_log;                  // close logging

private:
    /**
     * @brief Private constructor (Singleton pattern)
     */
    LOG();
    /**
     * @brief Destructor - releases resources
     */
    virtual ~LOG();
    /**
     * @brief Asynchronous log writer (consumer thread)
     * @return void* (pthread-compatible)
     * @details
     * - Continuously pops messages from m_log_queue
     * - Writes to file with mutex protection
     * - Terminates when queue is empty and closed
     */
    void *async_write_log()
    {
        string single_log;

        while (m_log_queue->pop(single_log))
        {
            m_mutex.lock();
            fputs(single_log.c_str(), m_fp); // write the string poped from blocking queue to the file using file pointer
            m_mutex.unlock();
        }
        return NULL;
    }

public:
    /**
     * @brief Get singleton instance (thread-safe in C++11+)
     * @return LOG* Pointer to singleton instance
     */
    static LOG *get_instance()
    {
        static LOG instance;
        return &instance;
    }
    /**
     * @brief Thread entry point for async logging
     * @param args Thread arguments (unused)
     * @return void*
     */
    static void *flush_log_thread(void *args)
    {
        LOG::get_instance()->async_write_log();
        return NULL;
    }
    /**
     * @brief Initialize logging system
     * @param filename Base path/filename for logs
     * @param close_log 0=enable, 1=disable
     * @param log_buf_size Format buffer size (default: 8192)
     * @param split_lines Max lines per file (default: 5M)
     * @param max_queue_size Async mode if >0 (0=sync)
     * @return bool True if initialization succeeded
     */
    bool init(const char *filename, int close_log, int log_buf_size = 8192, int split_lines = 5000000, int max_queue_size = 0);

    /**
     * @brief Write formatted log message
     * @param level Log level (0=DEBUG,1=INFO,2=WARN, 3=ERROR)
     * @param format printf-style format string
     * @param ... Variable arguments for format
     * @details
     * - Adds timestamp and log level prefix
     * - For async: Enqueues message
     * - For sync: Direct write with mutex
     * - Handles log file rotation
     */
    void write_log(int level, const char *format, ...);

    /**
     * @brief Flush buffers to disk
     * @note Thread-safe forced flush
     */
    void flush(void);
};

/**
 * @def LOG_DEBUG(format, ...)
 * @brief Debug-level log (level 0)
 * @note Disabled if m_close_log != 0
 */
#define LOG_DEBUG(format, ...)                                    \
    if (0 == m_close_log)                                         \
    {                                                             \
        LOG::get_instance()->write_log(0, format, ##__VA_ARGS__); \
        LOG::get_instance()->flush();                             \
    }

/**
 * @def LOG_INFO(format, ...)
 * @brief Debug-level log (level 0)
 * @note Disabled if m_close_log != 0
 */
#define LOG_INFO(format, ...)                                     \
    if (0 == m_close_log)                                         \
    {                                                             \
        LOG::get_instance()->write_log(1, format, ##__VA_ARGS__); \
        LOG::get_instance()->flush();                             \
    }

/**
 * @def LOG_WARN(format, ...)
 * @brief Debug-level log (level 0)
 * @note Disabled if m_close_log != 0
 */
#define LOG_WARN(format, ...)                                     \
    if (0 == m_close_log)                                         \
    {                                                             \
        LOG::get_instance()->write_log(2, format, ##__VA_ARGS__); \
        LOG::get_instance()->flush();                             \
    }
/**
 * @def LOG_ERROR(format, ...)
 * @brief Error-level log (level 3)
 * @note Always flushes immediately
 */
#define LOG_ERROR(format, ...)                                    \
    if (0 == m_close_log)                                         \
    {                                                             \
        LOG::get_instance()->write_log(3, format, ##__VA_ARGS__); \
        LOG::get_instance()->flush();                             \
    }

#endif