#ifndef _TIMER_H_
#define _TIMER_H_

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <time.h>
#include "../lock/locker.h"

class UTIL_TIMER;

/**
 * @struct client_data
 * @brief Client connection information bound to timers
 */
struct client_data
{
    sockaddr_in address; ///< Client socket address
    int sockfd;          ///< Client socket file descriptor
    UTIL_TIMER *timer;   ///< Associated timer object
};

/**
 * @class UTIL_TIMER
 * @brief Timer node for doubly-linked list
 *
 * Each timer contains:
 * - Expiration time
 * - Callback function
 * - Client data
 * - Previous/next pointers
 */
class UTIL_TIMER
{
public:
    UTIL_TIMER() : prev(NULL), next(NULL) {}

public:
    time_t expire; ///< Absolute expiration timestamp

    void (*cb_func)(client_data *); ///< Callback function pointer
    client_data *user_data;         ///< Associated client data
    UTIL_TIMER *prev;               ///< Previous timer in list
    UTIL_TIMER *next;               ///< Next timer in list
};

/**
 * @class SORT_TIMER_lST
 * @brief Sorted doubly-linked list of timers (ascending order)
 *
 * Features:
 * - O(n) insertion (worst-case)
 * - O(1) deletion
 * - O(1) tick processing
 */
class SORT_TIMER_lST
{
private:
    /**
     * @brief Internal helper for sorted insertion
     * @param timer Timer to insert
     * @param lst_head Starting point for insertion search
     */
    void add_timer(UTIL_TIMER *timer, UTIL_TIMER *lst_head);

    UTIL_TIMER *head; ///< List head (earliest expiration)
    UTIL_TIMER *tail; ///< List tail (latest expiration)

public:
    SORT_TIMER_lST();
    ~SORT_TIMER_lST();

    /**
     * @brief Add timer to sorted list
     * @param timer Timer to add
     */
    void add_timer(UTIL_TIMER *timer);

    /**
     * @brief Adjust timer position after expiration time change
     * @param timer Timer to adjust
     */
    void adjust_timer(UTIL_TIMER *timer);

    /**
     * @brief Remove timer from list
     * @param timer Timer to delete
     */
    void delete_timer(UTIL_TIMER *timer);

    /**
     * @brief Process expired timers
     * @note Calls callback functions and removes expired timers
     */
    void tick();
};

/**
 * @class TIMER_UTILS
 * @brief Manages timer events and signal handling
 *
 * Key Responsibilities:
 * - Timer list management
 * - Signal handling setup
 * - Non-blocking I/O configuration
 * - Epoll event management
 */
class UTILS
{
public:
    static int *u_pipefd;       ///< Pipe for signal notifications
    SORT_TIMER_lST m_timer_lst; ///< Active timer list
    static int u_epollfd;       ///< Epoll file descriptor
    int m_timeslot;             ///< Default timeout duration (seconds)

public:
    UTILS() {}
    ~UTILS() {}

    /**
     * @brief Initialize timer utilities
     * @param timeslot Default timeout value (seconds)
     */
    void init(int timeslot);

    /**
     * @brief Set file descriptor to non-blocking mode
     * @param fd File descriptor to modify
     * @return Original flags on success, -1 on error
     */
    int set_non_blocking(int fd);

    /**
     * @brief Add fd to epoll monitoring
     * @param epollfd Epoll instance
     * @param fd File descriptor to add
     * @param one_shot Enable one-shot mode
     * @param trigger_mode 0=Level-triggered, 1=Edge-triggered
     */
    void addfd(int epollfd, int fd, bool one_shot, int trigger_mode);

    /**
     * @brief Signal handler (static)
     * @param sig Received signal
     */
    static void sig_handler(int sig);

    /**
     * @brief Register signal handler
     * @param sig Signal number
     * @param handler Signal handler function
     * @param restart Enable SA_RESTART flag
     */
    void addsig(int sig, void(handler)(int), bool restart = true);

    /**
     * @brief Process timer events
     * @note Calls tick() on timer list and handles alarms
     */
    void time_handler();

    /**
     * @brief Send error message to client
     * @param connfd Client socket
     * @param info Error message text
     */
    void show_error(int connfd, const char *info);
};

/**
 * @brief Default timer callback function
 * @param user_data Client data bound to expired timer
 * @note Closes socket and removes from epoll
 */
void cb_func(client_data *user_data);

#endif