/**
 * NOTE:
 *
 */

#include "connection_pool.h"
#include <stdlib.h>
#include <string>
#include <string.h>
#include <stdio.h>
#include <list>
#include <pthread.h>
#include <iostream>

using namespace std;

DB_CONNECTION_POOL::DB_CONNECTION_POOL()
{
    m_curr_conn = 0;
    m_free_conn = 0;
}
DB_CONNECTION_POOL::~DB_CONNECTION_POOL()
{
    destroy_conn_pool();
}

DB_CONNECTION_POOL *DB_CONNECTION_POOL::get_instance()
{
    static DB_CONNECTION_POOL conn_pool;
    return &conn_pool;
}

void DB_CONNECTION_POOL::init(string url, string user, string password, string db_name, int port, int max_conn, int close_log)
{
    m_url = url;
    m_port = port;
    m_user = user;
    m_password = password;
    m_db_name = db_name;
    m_close_log = m_close_log;

    for (int i = 0; i < max_conn; i++)
    {
        MYSQL *con = NULL;
        // put all the config data to address
        con = mysql_init(con);

        if (con == NULL)
        {
            LOG_ERROR("Mysql Error");
            exit(1);
        }
        con = mysql_real_connect(con, url.c_str(), user.c_str(), password.c_str(), db_name.c_str(), port, NULL, 0);
        if (con == NULL)
        {
            LOG_ERROR("MySQL Error");
        }
        conn_list.push_back(con);
        ++m_free_conn;
    }
    reserve = SEM(m_free_conn);
    m_max_conn = m_free_conn;
}
MYSQL *DB_CONNECTION_POOL::get_conn()
{
    MYSQL *con = NULL;
    if (0 == conn_list.size())
        return NULL;
    reserve.wait();

    lock.lock();

    con = conn_list.front();
    conn_list.pop_front();

    --m_free_conn;
    ++m_curr_conn;

    lock.unlock();
    return con;
}

bool DB_CONNECTION_POOL::release_conn(MYSQL *conn)
{
    if (conn == NULL)
        return false;
    lock.lock();

    conn_list.push_back(conn);
    ++m_free_conn;
    --m_curr_conn;

    lock.unlock();

    reserve.post();
    return true;
}

void DB_CONNECTION_POOL::destroy_conn_pool()
{
    lock.lock();
    if (conn_list.size() > 0)
    {
        list<MYSQL *>::iterator it;
        for (it = conn_list.begin(); it != conn_list.end(); ++it)
        {
            MYSQL *con = *it;
            mysql_close(con);
        }
        m_free_conn = 0;
        m_curr_conn = 0;
        conn_list.clear();
    }
    lock.unlock();
}

int DB_CONNECTION_POOL::get_free_count()
{
    return this->m_free_conn;
}

// Double pointer is used because if we want to modify a pointer we have to use double pointer
CONNECTION_POOL_RAII::CONNECTION_POOL_RAII(MYSQL **SQL, DB_CONNECTION_POOL *conn_pool)
{
    *SQL = conn_pool->get_conn();

    // this two variable is used to release resources
    conRAII = *SQL;
    poolRAII = conn_pool;
}

CONNECTION_POOL_RAII::~CONNECTION_POOL_RAII()
{
    poolRAII->release_conn(conRAII);
}