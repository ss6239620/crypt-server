#ifndef BLOCK_QUEUE_H
#define BLOCK_QUEUE_H

#include <iostream>
#include <stdlib.h>
#include <pthread.h>
#include <sys/time.h>
#include "../lock/locker.h"

using namespace std;

/**
 * @class block_queue
 * @brief Thread-safe circular blocking queue implementation using POSIX synchronization primitives.
 * @tparam T Type of elements stored in the queue.
 */
template <class T>
class block_queue
{
private:
    LOCKER m_mutex;   ///< Mutex for thread synchronization
    CONDITION m_cond; ///< Condition variable for blocking operations

    T *m_array;     ///< Internal array storing queue elements
    int m_size;     ///< Current number of elements in queue
    int m_max_size; ///< Maximum capacity of the queue
    int m_front;    ///< Index of front element (for popping)
    int m_back;     ///< Index of back element (for pushing)

public:
    /**
     * @brief Constructs a blocking queue with specified maximum size.
     * @param max_size Maximum capacity of the queue (default: 1000).
     * @note Terminates program if max_size <= 0.
     */
    block_queue(int max_size = 1000)
    {
        if (max_size <= 0)
        {
            exit(-1);
        }
        m_max_size = max_size;
        m_array = new T[max_size];
        m_size = 0;
        m_front = -1;
        m_back = -1;
    }
    /**
     * @brief Destructor - releases allocated memory.
     */
    ~block_queue()
    {
        m_mutex.lock();
        if (m_array != NULL)
            delete[] m_array;
        m_mutex.unlock();
    }
    /**
     * @brief Clears all elements from the queue.
     */
    void clear()
    {
        m_mutex.lock();
        m_front = -1;
        m_size = 0;
        m_back = -1;
        m_mutex.unlock();
    }
    /**
     * @brief Checks if queue is full.
     * @return true if queue is full, false otherwise.
     */
    bool full()
    {
        m_mutex.lock();
        if (m_size >= m_max_size)
        {
            m_mutex.unlock();
            return true;
        }
        m_mutex.unlock();
        return false;
    }
    /**
     * @brief Checks if queue is empty.
     * @return true if queue is empty, false otherwise.
     */
    bool empty()
    {
        m_mutex.lock();
        if (0 == m_size)
        {
            m_mutex.unlock();
            return true;
        }
        m_mutex.unlock();
        return false;
    }
    /**
     * @brief Retrieves front element without removing it.
     * @param[out] value Reference to store the front element.
     * @return true if successful, false if queue is empty.
     */
    bool front(T &value)
    {
        m_mutex.lock();
        if (0 == m_size)
        {
            m_mutex.unlock();
            return false;
        }
        value = m_array[m_front];
        m_mutex.unlock();
        return true;
    }
    /**
     * @brief Retrieves back element without removing it.
     * @param[out] value Reference to store the back element.
     * @return true if successful, false if queue is empty.
     */
    bool back(T &value)
    {
        m_mutex.lock();
        if (m_size == 0)
        {
            m_mutex.unlock();
            return false;
        }
        value = m_array[m_back];
        m_mutex.unlock();
        return true;
    }
    /**
     * @brief Gets current number of elements in queue.
     * @return Current queue size.
     */
    int size()
    {
        int tmp = 0;
        m_mutex.lock();
        tmp = m_size;
        m_mutex.unlock();
        return tmp;
    }
    /**
     * @brief Adds an element to the queue (non-blocking).
     * @param item Element to be added.
     * @return true if successful, false if queue is full.
     * @note Wakes up all waiting threads after adding.
     */
    bool push(const T &item)
    {
        m_mutex.lock();
        if (m_size >= m_max_size)
        {
            m_cond.broadcast(); // Awaken all the consumer
            m_mutex.unlock();
            return false;
        }
        m_back = (m_back + 1) % m_max_size;
        m_array[m_back] = item;
        m_size++;

        m_cond.broadcast();
        m_mutex.unlock();
        return true;
    }
    /**
     * @brief Removes and returns front element (blocking).
     * @param[out] item Reference to store the removed element.
     * @return true if successful, false on error.
     * @note Blocks indefinitely until element is available.
     */
    bool pop(T &item)
    {
        m_mutex.lock();
        while (m_size <= 0)
        {
            if (!m_cond.wait(m_mutex.get()))
            {
                m_mutex.unlock();
                return false;
            }
        }
        m_front = (m_front + 1) % m_max_size;
        item = m_array[m_front];
        m_size--;

        m_mutex.unlock();
        return true;
    }
    /**
     * @brief Removes and returns front element with timeout.
     * @param[out] item Reference to store the removed element.
     * @param ms_timeout Maximum time to wait (in milliseconds).
     * @return true if successful, false on timeout or error.
     */
    bool pop(T &item, int ms_timeout)
    {
        struct timespec t = {0, 0};
        struct timeval now = {0, 0};
        gettimeofday(&now, NULL);
        m_mutex.lock();
        if (m_size <= 0)
        {
            t.tv_sec = now.tv_sec + ms_timeout / 1000;
            t.tv_nsec = (ms_timeout % 1000) * 1000;
            if (!m_cond.timewait(m_mutex.get(), t))
            {
                m_mutex.unlock();
                return false;
            }
        }
        if (m_size <= 0)
        {
            m_mutex.unlock();
            return false;
        }
        m_front = (m_front + 1) % m_max_size;
        item = m_array[m_front];
        m_size--;
        m_mutex.unlock();
        return true;
    }
};

#endif