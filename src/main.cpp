#include "config/config.h"
#include "http/http_routes.h"
#include <cstdlib>
#include <iostream>
#include <string>

namespace
{
string env_or_default(const char *name, const char *fallback)
{
    const char *value = getenv(name);
    return (value && value[0] != '\0') ? string(value) : string(fallback);
}

int env_int_or_default(const char *name, int fallback)
{
    const char *value = getenv(name);
    return (value && value[0] != '\0') ? atoi(value) : fallback;
}
}

int main(int argc, char *argv[])
{
    string db_host = env_or_default("DB_HOST", "127.0.0.1");
    int db_port = env_int_or_default("DB_PORT", 3306);
    string user = env_or_default("DB_USER", "crypt_user");
    string password = env_or_default("DB_PASSWORD", "crypt_password");
    string db_name = env_or_default("DB_NAME", "crypt_server");

    ROUTER &router = ROUTER::get_instance();

    router.make_static("/root");

    // Register routes
    router.get("/", [](const HttpRequest &req, HttpResponse &res)
               { res.send(200, "Hello from root!"); });

    router.get("/about", [](const HttpRequest &req, HttpResponse &res)
               { res.send(200, "About page"); });

    router.post("/login", [](const HttpRequest &req, HttpResponse &res){
        res.send(200,"kaisa hai bhai");
    });

    router.get("/contact", [](const HttpRequest &req, HttpResponse &res)
               { res.render(200, "/video.html"); });

    router.post("/login", [](const HttpRequest &req, HttpResponse &res) {
    MYSQL *mysql = NULL;
    CONNECTION_POOL_RAII mysqlcon(&mysql, DB_CONNECTION_POOL::get_instance());

    if (mysql == NULL) {
        res.send(503, "Database busy");
        return;
    }

    // mysql_query(mysql, ...)

    res.send(200, "login ok");
});

    CONFIG config;
    config.parse_arg(argc, argv);

    WEBSERVER server;

    server.init(config.port, db_host, db_port, user, password, db_name, config.log_write, config.opt_linger, config.trigger_mode, config.sql_num, config.thread_num, config.close_log, config.actor_model);

    server.log_write();

    server.sql_pool();

    server.thread_pool();

    server.trigger_mode();

    server.event_listen();

    cout << "Server started.." << endl;

    server.event_loop();

    return 0;
}
