#ifndef _CONGIG_H_
#define _CONGIG_H_

#include "../webserver/webserver.h"

using namespace std;

/**
 * @class CONFIG
 * @brief Server configuration manager that handles:
 * - Command line argument parsing
 * - Runtime configuration storage
 * - Default value management
 */
class CONFIG
{
public:
    /**
     * @brief Constructor - initializes default values
     */
    CONFIG();

    /**
     * @brief Destructor
     */
    ~CONFIG() {};

    /**
     * @brief Parse command line arguments
     * @param argc Argument count
     * @param argv Argument vector
     * @note Expected arguments:
     * -p <port>          Server port
     * -l <0|1>           Log write mode (0:sync, 1:async)
     * -m <0|1>           Trigger mode (0:LT, 1:ET)
     * -o <0|1>           Opt linger
     * -s <sql_num>       SQL connection pool size
     * -t <thread_num>    Thread pool size
     * -c <0|1>           Close log (0:enable, 1:disable)
     * -a <0|1>           Actor model (0:Proactor, 1:Reactor)
     */
    void parse_arg(int argc, char *argv[]);

    /* Server configuration parameters */

    int port;                ///< Listening port (default: 9006)
    int log_write;           ///< Logging mode (0:sync, 1:async)
    int trigger_mode;        ///< Global trigger mode (0:LT, 1:ET)
    int listen_trigger_mode; ///< Listener trigger mode (0:LT, 1:ET)
    int conn_trigger_mode;   ///< Connection trigger mode (0:LT, 1:ET)
    int opt_linger;          ///< Linger option (0:off, 1:on)
    int sql_num;             ///< SQL connection pool size (default: 8)
    int thread_num;          ///< Thread pool size (default: 8)
    int close_log;           ///< Logging enable (0) or disable (1)
    int actor_model;         ///< Concurrency model (0:Proactor, 1:Reactor)
};

#endif