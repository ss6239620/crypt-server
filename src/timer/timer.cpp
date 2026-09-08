/**
 * NOTES:
 * - sig_handler is executed whenever a signal (e.g., SIGINT, SIGTERM) is received.
 * - Signals are asynchronous, meaning they can interrupt the program at any moment.
 * - The signal number (sig) is written to a pipe descriptor (u_pipefd[1]).
 * - This allows another part of the program (e.g., an event loop) to read and process the signal
 *   later in a controlled manner.
 *
 * @example
 * - Suppose the program receives SIGINT (Ctrl+C).
 * - sig_handler(2) executes (SIGINT is signal number 2).
 * - The message 2 is written to u_pipefd[1].
 * - Another part of the program (probably an epoll-based event loop) will read from
 *   u_pipefd[0] and handle it properly.
 *
 * NOTES:
 * time_handler() This function handles periodic timer events.
 * It is likely called when an alarm signal (SIGALRM) is received.
 * m_timer_lst.tick() processes all expired timers.
 * alarm(m_timeslot) resets the alarm so that SIGALRM is triggered again after m_timeslot seconds.
 * PURPOSE:
 * - Ensures periodic timeout handling (e.g., closing inactive connections).
 * - Works with a linked list of timers (m_timer_lst), where each timer tracks client activity.
 * - Automatically schedules the next alarm with alarm(m_timeslot).
 */

#include "timer.h"
#include "../http/http_coonection.h"

SORT_TIMER_lST::SORT_TIMER_lST()
{
    head = NULL;
    tail = NULL;
}

SORT_TIMER_lST::~SORT_TIMER_lST()
{
    UTIL_TIMER *tmp = head;
    while (tmp)
    {
        head = tmp->next;
        delete tmp;
        tmp = head;
    }
}

void SORT_TIMER_lST::add_timer(UTIL_TIMER *timer)
{
    if (!timer) // if timer is null return
        return;

    if (!head) // if list is empty timer list will contain 1 timer
    {
        head = tail = timer;
        return;
    }
    if (timer->expire < head->expire) // if timer expiartion is less than head expiration place it at head
    {
        timer->next = head;
        head->prev = timer;
        head = timer;
    }
    // if timer is not handled by above condition that means it should be placed in between middle.
    add_timer(timer, head);
}

void SORT_TIMER_lST::adjust_timer(UTIL_TIMER *timer)
{
    if (!timer)
        return;
    UTIL_TIMER *tmp = timer->next; // find the next timer of current timer

    // if it is null or timer expiration is less than next timer expiration no adjustment needed return
    if (!tmp || (timer->expire < tmp->expire))
        return;

    // if timer is head than we make the next element of a header as a head and add the timer again with the new head
    if (timer == head)
    {
        head = head->next;
        head->prev = NULL;
        timer->next = NULL;
        add_timer(timer, head);
    }
    // Here if above condition does not meet that means timer is in between head and tail we simply remove that timer from its position and as the list is sorted we will add that timer again from that point
    else
    {
        timer->prev->next = timer->next;
        timer->next->prev = timer->prev;
        add_timer(timer, timer->next);
    }
}

void SORT_TIMER_lST::delete_timer(UTIL_TIMER *timer)
{
    if (!timer)
        return;

    if ((timer == head) && (timer == tail)) // if timer is only single item than it is easily deleted
    {
        delete timer;
        head = NULL;
        tail = NULL;
        return;
    }
    if (timer == head) // when timer is head delete it from the list.
    {
        head = head->next;
        head->prev = NULL;
        delete timer;
        return;
    }
    if (timer == tail) // when timer is tail remove it from the list,
    {
        tail = tail->prev;
        tail->next = NULL;
        delete timer;
        return;
    }
    // if any of the above condition is not satisfied than just remove link of timer from the list and delete it.
    timer->next->prev = timer->prev;
    timer->prev->next = timer->next;
    delete timer;
}

void SORT_TIMER_lST::tick()
{
    if (!head)
        return;

    time_t cur = time(NULL); // current time
    UTIL_TIMER *tmp = head;

    while (tmp)
    {
        // if current time is less than any of the timer node than break the loop as rest of the timer will obiously be greater than current time as list is sorted.
        if (cur < tmp->expire)
            break;

        tmp->cb_func(tmp->user_data); // call the callback function
        head = tmp->next;             // make the tmp->next as head

        if (head)
            head->prev = NULL;

        delete tmp;
        tmp = head;
    }
}

void SORT_TIMER_lST::add_timer(UTIL_TIMER *timer, UTIL_TIMER *lst_head)
{
    UTIL_TIMER *prev = lst_head;
    UTIL_TIMER *tmp = prev->next;

    while (tmp)
    {
        if (timer->expire < tmp->expire)
        {
            prev->next = timer;
            timer->next = tmp;
            tmp->prev = timer;
            timer->prev = prev;
        }
        prev = tmp;
        tmp = tmp->next;
    }
    if (!tmp)
    {
        prev->next = timer;
        timer->prev = prev;
        timer->next = NULL;
        tail = timer;
    }
}

void UTILS::init(int time_slot)
{
    m_timeslot = time_slot;
}

int UTILS::set_non_blocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL); // get old flags of file descriptor
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option); // set new flag
    return old_option;
}

void UTILS::addfd(int epollfd, int fd, bool one_shot, int triger_mode)
{
    epoll_event event;
    event.data.fd = fd;

    /**
     * EPOLLIN: wait for input on file descriptor
     * EPOLLET: Make Epoll Edge Triggered (Blocking Synchronous operation)
     * EPOLLRDHUP: Remove file descriptor from epoll when connection is removed
     * EPOLLONESHOT: Send Once and be removed from epoll if want to use again we have to create it again
     */

    if (1 == triger_mode) // edge triggered
        event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    else // level triggered
        event.events = EPOLLIN | EPOLLRDHUP;

    if (one_shot)
        event.events |= EPOLLONESHOT;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event); // finally add to epoll
    set_non_blocking(fd);
}

void UTILS::sig_handler(int sig)
{
    // save error for reentrancy
    int save_error = errno;
    int msg = sig;
    send(u_pipefd[1], (char *)&msg, 1, 0); // Write the signal number into the pipe
    errno = save_error;
}

void UTILS::addsig(int sig, void(handler)(int), bool restart)
{
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;

    if (restart) // Restart system calls if interrupted
        sa.sa_flags |= SA_RESTART;
    sigfillset(&sa.sa_mask); // Block other signals while handling this one
    assert(sigaction(sig, &sa, NULL) != -1);
}

void UTILS::time_handler()
{
    m_timer_lst.tick(); // Trigger timer list events and process expred timers.
    alarm(m_timeslot);  // Reset alarm for the next timeslot
}

void UTILS::show_error(int connfd, const char *info)
{
    send(connfd, info, strlen(info), 0);
    close(connfd);
}

int *UTILS::u_pipefd = 0;
int UTILS::u_epollfd = 0;

class UTILS;
void cb_func(client_data *user_data)
{
    epoll_ctl(UTILS::u_epollfd, EPOLL_CTL_DEL, user_data->sockfd, 0); // delete from epoll
    assert(user_data);
    close(user_data->sockfd);  // close the socket
    HTTP_CONN::m_user_count--; // reduce user count
}