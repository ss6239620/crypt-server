#ifndef LOCKER_H
#define LOCKER_H

#include <exception>
#include <pthread.h>
#include <semaphore.h>

/**
 * @class SEM
 * @brief A simple C++ wrapper for POSIX semaphores (sem_init, sem_wait, sem_post, sem_destroy).
 */
class SEM
{
private:
    sem_t m_sem; ///< Underlying POSIX semaphore object.

public:
    /**
     * @brief Default constructor (initializes semaphore with count = 0).
     * @throws std::exception if sem_init() fails.
     */
    SEM()
    {
        if (sem_init(&m_sem, 0, 0) != 0)
            throw std::exception();
    }
    /**
     * @brief Constructor with custom initial count.
     * @param num Initial semaphore count.
     * @throws std::exception if sem_init() fails.
     */
    SEM(int num)
    {
        if (sem_init(&m_sem, 0, num) != 0)
            throw std::exception();
    }
    /**
     * @brief Destructor (releases semaphore resources).
     */
    ~SEM()
    {
        sem_destroy(&m_sem);
    }
    /**
     * @brief Decrements (locks) the semaphore (blocks if count is 0).
     * @return true if successful, false on error (e.g., interrupted by signal).
     */
    bool wait()
    {
        return sem_wait(&m_sem) == 0;
    }
    /**
     * @brief Increments (unlocks) the semaphore.
     * @return true if successful, false on error.
     */
    bool post()
    {
        return sem_post(&m_sem) == 0;
    }
};

/**
 * @class LOCKER
 * @brief A lightweight C++ wrapper for POSIX mutexes (pthread_mutex_t).
 *        Provides RAII-style management for thread synchronization.
 */
class LOCKER
{
private:
    pthread_mutex_t m_mutex; ///< Underlying POSIX mutex object.

public:
    /**
     * @brief Default constructor initializes the mutex.
     * @throws std::exception if pthread_mutex_init() fails.
     */
    LOCKER()
    {
        if (pthread_mutex_init(&m_mutex, NULL) != 0)
            throw std::exception();
    }
    /**
     * @brief Destructor releases mutex resources.
     * @note No-throw guarantee (errors are typically ignored during destruction).
     */
    ~LOCKER()
    {
        pthread_mutex_destroy(&m_mutex);
    }
    /**
     * @brief Locks the mutex (blocks if already locked by another thread).
     * @return true if successful, false on error (e.g., deadlock or invalid mutex).
     */
    bool lock()
    {
       return pthread_mutex_lock(&m_mutex) == 0;
    }
    /**
     * @brief Unlocks the mutex.
     * @return true if successful, false on error (e.g., unlocking an unlocked mutex).
     */
    bool unlock()
    {
       return pthread_mutex_unlock(&m_mutex) == 0;
    }
    /**
     * @brief Provides direct access to the underlying pthread_mutex_t.
     * @return Raw pointer to the POSIX mutex (for use with pthread_cond_wait, etc.).
     * @warning Use with caution to avoid breaking RAII semantics.
     */
    pthread_mutex_t *get()
    {
        return &m_mutex;
    }
};

/**
 * @class CONDITION
 * @brief A C++ wrapper for POSIX condition variables (pthread_cond_t).
 *        Provides thread synchronization mechanisms for waiting/signaling events.
 */
class CONDITION
{
private:
    pthread_cond_t m_cond; ///< Underlying POSIX condition variable.

public:
    /**
     * @brief Initializes the condition variable.
     * @throws std::exception if pthread_cond_init() fails.
     */
    CONDITION()
    {
        if (pthread_cond_init(&m_cond, NULL) != 0)
            throw std::exception();
    }
    /**
     * @brief Destroys the condition variable.
     * @note Safe to call even if threads are waiting (behavior is OS-dependent).
     */
    ~CONDITION()
    {
        pthread_cond_destroy(&m_cond);
    }
    /**
     * @brief Blocks the calling thread until signaled (requires locked mutex).
     * @param m_mutex Pointer to a locked pthread_mutex_t (must be held by caller).
     * @return true if successful, false on error (e.g., invalid mutex).
     * @warning Always call this in a loop checking the predicate to avoid spurious wakeups.
     * @example
     *   mtx.lock();
     *   while (!predicate) cond.wait(mtx.get());
     *   mtx.unlock();
     */
    bool wait(pthread_mutex_t *m_mutex)
    {
        int ret = 0;
        ret = pthread_cond_wait(&m_cond, m_mutex);
        return ret == 0;
    }
    /**
     * @brief Blocks until signaled or the specified timeout expires.
     * @param m_mutex Pointer to a locked pthread_mutex_t.
     * @param t Absolute timeout (use clock_gettime(CLOCK_REALTIME, &ts) for setup).
     * @return true if signaled, false on timeout/error.
     * @note On timeout, the mutex is reacquired before returning.
     */
    bool timewait(pthread_mutex_t *m_mutex, struct timespec t)
    {
        int ret = 0;
        ret = pthread_cond_timedwait(&m_cond, m_mutex, &t);
        return ret == 0;
    }
    /**
     * @brief Wakes up at least one waiting thread.
     * @return true if successful, false on error.
     * @note Order of wakeups is not guaranteed.
     */
    bool signal()
    {
        return pthread_cond_signal(&m_cond);
    }
    /**
     * @brief Wakes up all waiting threads.
     * @return true if successful, false on error.
     * @warning May cause "thundering herd" effect - use judiciously.
     */
    bool broadcast()
    {
        return pthread_cond_broadcast(&m_cond);
    }
};

#endif