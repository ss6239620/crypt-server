/**
 * Understanding ACTOR and PROACTOR Model for Concurrent programming
 *
 * Proactor model: https://medium.com/@beeleeong/the-proactor-pattern-bbfe7c33a43c
 * Actor Mode: https://medium.com/@KtheAgent/actor-model-in-nutshell-d13c0f81c8c7
 *
 * This thread pool implementation actually supports both Actor and Proactor models,
 * selectable via the m_actor_model flag in the constructor
 *
 * Here we use Model when
 * Actor mode if:
 * - Tasks require stateful processing
 * - I/O is synchronous (e.g., blocking DB queries)
 *
 * Proactor mode if:
 * - Using async I/O (e.g., epoll + non-blocking sockets)
 * - Separating I/O and computation phases
 */

#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <list>
#include <cstdio>
#include <exception>
#include <pthread.h>
#include "../lock/locker.h"
#include "../cgi_mysql/connection_pool.h"

using namespace std;

/**
 * @class THREADPOOL
 * @brief Thread pool implementation with task queue and database connection pooling
 * @tparam T Type of work items processed by threads
 *
 * Features:
 * - Fixed number of worker threads
 * - Bounded task queue with semaphore synchronization
 * - Supports both synchronous (actor) and asynchronous modes
 * - Integrated database connection pooling
 */
template <typename T>
class THREADPOOL
{
private:
    /* data */
    int m_thread_num;                ///< Number of worker threads
    int m_max_request;               ///< Maximum queue capacity
    pthread_t *m_threads;            ///< Array of thread IDs
    list<T *> m_work_queue;          ///< Task queue (FIFO)
    LOCKER m_queue_locker;           ///< Mutex for queue access
    SEM m_queuestat;                 ///< Semaphore for task counting
    DB_CONNECTION_POOL *m_conn_pool; ///< Database connection pool
    int m_actor_model;               ///< 0=Proactor, 1=Actor

private:
    /**
     * @brief Static thread entry point
     * @param args Pointer to threadpool instance
     * @return void* returns any type of datatype pointers
     */
    static void *worker(void *args)
    {
        THREADPOOL *pool = (THREADPOOL *)args; // cast args to Threadpool class
        pool->run();                           // run the program
        return pool;
    }

    /**
     * @brief Main worker thread processing loop
     */
    void run()
    {
        while (true) // run for infinity dont worry most of the time they are asleep
        {
            // check if any work is there in queue if there is than proess it otherwise wait.
            m_queuestat.wait();
            m_queue_locker.lock();

            // check if some othere worker has already processed data in queue as their are 8 workeer in total we have to check if the request is alraedy consumed if it is we have to unlock the lock and continue.
            if (m_work_queue.empty())
            {
                m_queue_locker.unlock();
                continue;
            }
            T *request = m_work_queue.front(); // value from front of work queue
            m_work_queue.pop_front();          // pop the value
            m_queue_locker.unlock();

            // here we also check if request is not null
            if (!request)
                continue;

            // actor mode
            if (1 == m_actor_model)
            {
                // if it is read operation i.e we are reading request.
                if (0 == request->m_state)
                {
                    if (request->read_once()) // if true then success
                    {
                        request->improv = 1; // Mark as "processed"
                        // acquire connection from db_pool and give attach it to request
                        CONNECTION_POOL_RAII mysqlcon(&request->mysql, m_conn_pool);
                        // now everything is ready process the data
                        request->process();
                    }
                    else // when rea_once fails
                    {
                        request->improv = 1;     // Still Mark as "processed"
                        request->timer_flag = 1; // But flag for cleanup
                    }
                }
                // if it is write operation i.e we are writing response.
                else
                {
                    if (request->write()) // when write succeed
                    {
                        request->improv = 1;
                    }
                    else // when write flag set timer flag for clean up database release will be done internally
                    {
                        request->improv = 1;
                        request->timer_flag = 1;
                    }
                }
            }
            // proactor model
            else
            {
                CONNECTION_POOL_RAII mysqlcon(&request->mysql, m_conn_pool); // acquire db and set it for request
                request->process();                                          // process the request we do not have to wait for read and write it will be handled internally.
            }
        }
    }

public:
    /**
     * @brief Construct a thread pool
     * @param actor_model 0=Proactor, 1=Actor mode
     * @param conn_pool Database connection pool
     * @param thread_num Number of worker threads (default=8)
     * @param max_request Maximum queue size (default=10000)
     */
    THREADPOOL(int actor_model, DB_CONNECTION_POOL *conn_pool, int thread_num = 8, int max_request = 10000) : m_actor_model(actor_model), m_thread_num(thread_num), m_conn_pool(conn_pool), m_max_request(max_request), m_threads(NULL)
    {
        if (thread_num <= 0 || max_request <= 0)
            throw exception();

        m_threads = new pthread_t[m_thread_num];
        if (!m_threads)
            throw exception();

        for (int i = 0; i < m_thread_num; i++)
        {
            // create thread the thread is found by moving 1 address on m_threads here worker cant accept params hence we pass it in 4th argument as this indicating the current instance
            if (pthread_create(m_threads + i, NULL, worker, this) != 0)
            {
                delete[] m_threads;
                throw exception();
            }
            // when thread terminate just directly release resourec instead of waiting
            if (pthread_detach(m_threads[i]))
            {
                delete[] m_threads;
                throw exception();
            }
        }
    }

    /**
     * @brief Destructor - stops all threads
     */
    ~THREADPOOL()
    {
        delete[] m_threads;
    }

    /**
     * @brief Add task to queue (Actor mode)
     * @param request Work item
     * @param state Task state flag
     * @return true if queued successfully
     * @return false if queue full
     */
    bool append(T *request, int state) // here state can be 0 - for read and 1 for write
    {
        m_queue_locker.lock();
        if (m_work_queue.size() >= m_max_request)
        {
            m_queue_locker.unlock();
            return false;
        }
        request->m_state = state; // save the state 0 for read and 1 for write
        m_work_queue.push_back(request);
        m_queue_locker.unlock();
        // increase semaphore to tell the worker threads that there is request in queue to process (everytime a request comes it will increate semaphore count by one and any of the worker will consume it).
        m_queuestat.post();
        return true;
    }

    /**
     * @brief Add task to queue (Proactor mode)
     * @param request Work item
     * @return true if queued successfully
     * @return false if queue full
     */
    bool append_p(T *request)
    {
        m_queue_locker.lock();
        if (m_work_queue.size() >= m_max_request)
        {
            m_queue_locker.unlock();
            return false;
        }
        m_work_queue.push_back(request);
        m_queue_locker.unlock();
        // increase semaphore to tell the worker threads that there is request in queue to process (everytime a request comes it will increate semaphore count by one and any of the worker will consume it).
        m_queuestat.post();
        return true;
    }
};

#endif