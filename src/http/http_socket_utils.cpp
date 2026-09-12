#include "http_socket_utils.h"

#include <fcntl.h>
#include <sys/epoll.h>
#include <unistd.h>

int set_non_blocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);
    // Add O_NONBLOCK while preserving the existing flags.
    // With non-blocking mode, recv()/send() never wait for I/O.
    // If no data is available, recv() returns EAGAIN/EWOULDBLOCK,
    // allowing us to stop reading and return control to epoll.
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
