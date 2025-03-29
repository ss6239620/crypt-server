#include "config/config.h"
#include "http/http_routes.h"
int main(int argc, char *argv[])
{
    string user = "sql12770026";
    string password = "BgQNaiI2Rb";
    string db_name = "sql12770026";

    ROUTER &router = ROUTER::get_instance();

    // Register routes
    router.get("/", [](const HttpRequest &req, HttpResponse &res)
               { res.send(200, "Hello from root!"); });

    router.get("/about", [](const HttpRequest &req, HttpResponse &res)
               { res.send(200, "About page"); });

    router.post("/login", [](const HttpRequest &req, HttpResponse &res)
                { res.send(200, "Login endpoint"); });

    router.get("/contact", [](const HttpRequest &req, HttpResponse &res)
               { res.render(200, "/video.html"); });

    CONFIG config;
    config.parse_arg(argc, argv);

    WEBSERVER server;

    server.init(config.port, user, password, db_name, config.log_write, config.opt_linger, config.trigger_mode, config.sql_num, config.thread_num, config.close_log, config.actor_model);

    server.log_write();

    server.sql_pool();

    server.thread_pool();

    server.trigger_mode();

    server.event_listen();

    cout << "Server started.." << endl;

    server.event_loop();

    return 0;
}