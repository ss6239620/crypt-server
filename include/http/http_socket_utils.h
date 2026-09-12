#ifndef HTTP_SOCKET_UTILS_H
#define HTTP_SOCKET_UTILS_H

/**
 * @brief Put a file descriptor in non-blocking mode.
 * @return The old descriptor flags returned by fcntl(F_GETFL).
 */
int set_non_blocking(int fd);

/**
 * @brief Add a client/listener fd to epoll.
 */
void addfd(int epollfd, int fd, bool one_shot, int trigger_mode);

/**
 * @brief Remove a fd from epoll and close it.
 */
void removefd(int epollfd, int fd);

/**
 * @brief Change an existing epoll fd to listen for a new event set.
 */
void modfd(int epollfd, int fd, int ev, int trigger_mode);

#endif
