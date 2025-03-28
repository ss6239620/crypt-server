/**
 * NOTE:
 * - "%02d" means an integer, left padded with zeros up to 2 digits.
 * - "%08ld" will append preceding 0’s up to 8 digits.
 * - "snprintf" this function return how much character it has return in buffer
 * - "vsnprintf" this function return how much character it has return in buffer but it has variable  argument
 */

#include <time.h>
#include <sys/time.h>
#include "log.h"

using namespace std;

LOG::LOG()
{
    m_count = 0;
    m_is_async = false;
}

LOG::~LOG()
{
    if (m_fp != NULL)
        fclose(m_fp);
}
bool LOG::init(const char *file_name, int close_log, int log_buf_size, int split_lines, int max_queue_size)
{
    // Asynchronous mode require setting the length of blocking queue and synchronous mode does not
    // if max_queue_size is greater than 1 we need asynchronoes writing method
    if (max_queue_size >= 1)
    {
        m_is_async = true;
        m_log_queue = new block_queue<string>(max_queue_size);
        pthread_t tid;
        // create a thread to process(consumer) asynchronous log  
        pthread_create(&tid, NULL, flush_log_thread, NULL);
    }
    m_close_log = close_log;
    m_log_buf_size = log_buf_size;
    m_buf = new char[m_log_buf_size];
    memset(m_buf, '\0', m_log_buf_size);
    m_split_line = split_lines;

    time_t t = time(NULL);
    struct tm *sys_tm = localtime(&t);
    struct tm my_tm = *sys_tm;

    const char *p = strrchr(file_name, '/'); //  searches for the last occurrence of a specified character eg /root/example.txt it points to /example.txt otherwise points to NULL
    char log_full_name[256] = {0};

    if (p == NULL)
        // year is stored as eg.2025-1900 as 125 so we need to add 1900 back also months are 0 index based we need to add 1
        snprintf(log_full_name, 255, "%d_%02d_%02d_%s", my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday, file_name);
    else
    {
        strcpy(log_name, p + 1);
        strncpy(dir_name, file_name, p - file_name + 1); // copy dir_name from file name if it contain dir name
        snprintf(log_full_name, 255, "%s%d_%02d_%02d_%s", dir_name, my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday, log_name);
    }
    m_today = my_tm.tm_mday;

    m_fp = fopen(log_full_name, "a");
    if (m_fp == NULL)
        return false;
    return true;
}

void LOG::write_log(int level, const char *format, ...)
{
    struct timeval now = {0, 0};
    gettimeofday(&now, NULL);
    time_t t = now.tv_sec;
    struct tm *sys_tm = localtime(&t);
    struct tm my_tm = *sys_tm;
    char s[16] = {0};
    switch (level)
    {
    case 0:
        strcpy(s, "[debug]:");
        break;
    case 1:
        strcpy(s, "[info]:");
        break;
    case 2:
        strcpy(s, "[warn]:");
        break;
    case 3:
        strcpy(s, "[error]:");
        break;
    default:
        strcpy(s, "[info]:");
        break;
    }
    // Write a log, for m_count+,m_split_lines maximum number of lines
    m_mutex.lock();
    m_count++;

    // first conditon is to chacke if today is not the same day
    // second condion is to check if file is filled completely as we have to make a new one
    if (m_today != my_tm.tm_mday || m_count % m_split_line == 0)
    {
        char new_log[255] = {0};
        fflush(m_fp); // flush the today fp stream
        fclose(m_fp); // close the today file
        char tail[16] = {0};

        snprintf(tail, 16, "%d_%02d_%02d_", my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday);

        // first condion
        if (m_today != my_tm.tm_mday)
        {
            snprintf(new_log, 255, "%s%s%s", dir_name, tail, log_name);
            m_today = my_tm.tm_mday;
            m_count = 0;
        }
        else // 2nd condition
            snprintf(new_log, 255, "%s%s%s.%lld", dir_name, tail, log_name, m_count / m_split_line);
        m_fp = fopen(new_log, "a");
    }
    m_mutex.unlock();

    // va_list is a type used for handling variable arguments
    va_list valst;
    va_start(valst, format); // Means that all arguments after format are stored in valst.

    string log_str;
    m_mutex.lock();

    // it adds current time and other info and level
    int n = snprintf(m_buf, 48, "%d-%02d-%02d %02d:%02d:%02d.%06ld %s", my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday, my_tm.tm_hour, my_tm.tm_min, my_tm.tm_sec, now.tv_usec, s);

    // add the user message
    int m = vsnprintf(m_buf + n, m_log_buf_size - n - 1, format, valst);

    m_buf[n + m] = '\n';     // add new line charactor
    m_buf[n + m + 1] = '\0'; // add end charactor
    log_str = m_buf;

    m_mutex.unlock();

    if (m_is_async && !m_log_queue->full()) // this check if we have to write asynchronisoly
    {
        m_log_queue->push(log_str); // add to buffer as a producer
    }
    else // just directly write to file i.e synchronously
    {
        m_mutex.lock();
        fputs(log_str.c_str(), m_fp);
        m_mutex.unlock();
    }
    va_end(valst); // clear the args list
}

void LOG::flush(void)
{
    m_mutex.lock();
    fflush(m_fp);
    m_mutex.unlock();
}