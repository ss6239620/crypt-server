#ifndef _CONNECTION_POOL_
#define _CONNECTION_POOL_

#include <list>
#include <mysql/mysql.h>
#include "../log/log.h"
#include "../lock/locker.h"

using namespace std;

/**
 * @class DB_CONNECTION_POOL
 * @brief MySQL database connection pool manager (thread-safe)
 *
 * Features:
 * - Singleton pattern for global access
 * - Connection reuse for performance
 * - Semaphore-controlled connection limiting
 * - RAII wrapper for automatic management
 */
class DB_CONNECTION_POOL
{
private:
    DB_CONNECTION_POOL();
    ~DB_CONNECTION_POOL();

    int m_max_conn;          ///< Maximum connection capacity
    int m_curr_conn;         ///< Current active connections
    int m_free_conn;         ///< Available idle connections
    LOCKER lock;             ///< Mutex for thread safety
    list<MYSQL *> conn_list; ///< Pool of MySQL connections
    SEM reserve;             ///< Semaphore for connection limiting

public:
    /**
     * @brief Acquire a database connection
     * @return MYSQL* Connection handle (NULL if failed)
     * @note Blocks if no connections available (when pool exhausted)
     */
    MYSQL *get_conn();
    /**
     * @brief Release a connection back to pool
     * @param conn Connection to release
     * @return true if successful, false if invalid connection
     */
    bool release_conn(MYSQL *conn);
    /**
     * @brief Get count of available connections
     * @return Number of free connections
     */
    int get_free_count();
    /**
     * @brief Destroy all connections in pool
     * @note Called automatically on destructor
     */
    void destroy_conn_pool();

    // Singleton access
    /**
     * @brief Get singleton instance
     * @return DB_CONNECTION_POOL* Global instance
     */
    static DB_CONNECTION_POOL *get_instance();
    /**
     * @brief Initialize connection pool
     * @param url Database server host
     * @param user Database username
     * @param password Database password
     * @param db_name Database name
     * @param port Database server port
     * @param max_conn Maximum pool size
     * @param close_log Disable logging if non-zero
     */
    void init(string url, string user, string password, string db_name, int port, int max_conn, int close_log);

public:
    string m_url;      ///< Database server host
    string m_port;     ///< Database server port
    string m_user;     ///< Database username
    string m_password; ///< Database password
    string m_db_name;  ///< Database name
    int m_close_log;   ///< Logging disable flag
};

/**
 * @class CONNECTION_POOL_RAII
 * @brief RAII wrapper for connection handling
 *
 * Automatically:
 * - Acquires connection on construction
 * - Releases connection on destruction
 */
class CONNECTION_POOL_RAII
{
private:
    MYSQL *conRAII;               ///< Managed connection
    DB_CONNECTION_POOL *poolRAII; ///< Source connection pool

public:
    /**
     * @brief Construct RAII wrapper
     * @param[out] conn Pointer to store acquired connection
     * @param conn_pool Connection pool source
     */
    CONNECTION_POOL_RAII(MYSQL **conn, DB_CONNECTION_POOL *conn_pool);
    /**
     * @brief Destructor - automatically releases connection
     */
    ~CONNECTION_POOL_RAII();
};

#endif