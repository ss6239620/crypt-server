#include "config/config.h"

int main(int argc, char *argv[])
{
    string user = "";
    string password = "";
    string db_name = "";

    CONFIG config;
    config.parse_arg(argc, argv);

    WEBSERVER server;

    server.init(config.port, user, password, db_name, config.log_write, config.opt_linger, config.trigger_mode, config.sql_num, config.thread_num, config.close_log, config.actor_model);

    server.log_write();

    server.sql_pool();

    server.thread_pool();

    server.trigger_mode();

    server.event_listen();

    cout<<"Server started.."<<endl;

    server.event_loop();

    return 0;
}