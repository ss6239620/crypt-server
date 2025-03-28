#include "config.h"

CONFIG::CONFIG()
{
    port = 9906;             // Default to unprivileged port range
    log_write = 0;           // 0 = Synchronous logging (blocking)
    trigger_mode = 0;        // 0 = Level-triggered (LT) mode globally
    listen_trigger_mode = 0; // LT mode for listener socket
    conn_trigger_mode = 0;   // LT mode for connections
    opt_linger = 0;          // 0 = Disable SO_LINGER (fast close)
    sql_num = 8;             // 0 = Will be set properly during init
    thread_num = 8;          // 0 = Will be auto-configured later
    close_log = 0;           // 0 = Enable logging
    actor_model = 0;         // 0 = Proactor pattern
}

/**
 * argc - number of args passed
 * argv - Parses the next option in argv.
 * opt - Hold next args flag
 * getopt - it returns option character 
 * optarg - This is s global variable (from <unistd.h>) that stores the value of the current option.
 */

void CONFIG::parse_arg(int argc, char *argv[])
{
    int opt;
    const char *str = "p:l:m:o:s:t:c:a:";
    while (opt = getopt(argc, argv, str) != -1)
    {
        switch (opt)
        {
        case 'p':
        {
            port = atoi(optarg);
            break;
        }
        case 'l':
        {
            log_write = atoi(optarg);
            break;
        }
        case 'm':
        {
            trigger_mode = atoi(optarg);
            break;
        }
        case 'o':
        {
            opt_linger = atoi(optarg);
            break;
        }
        case 's':
        {
            sql_num = atoi(optarg);
            break;
        }
        case 't':
        {
            thread_num = atoi(optarg);
            break;
        }
        case 'c':
        {
            close_log = atoi(optarg);
            break;
        }
        case 'a':
        {
            actor_model = atoi(optarg);
            break;
        }
        default:
            break;
        }
    }
}