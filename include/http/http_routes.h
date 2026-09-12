#ifndef _ROUTES_H_
#define _ROUTES_H_

#include <unordered_map>
#include <functional>
#include <string>
#include <iostream>

#include "../lock/locker.h"
#include "../log/log.h"
#include "http_types.h"

// Alias for handler function type
using RouteHandler = function<void(const HttpRequest &, HttpResponse &)>;

class ROUTER
{
private:
    // Private constructor for singleton
    ROUTER() = default;
    // Map to store routes: key = "METHOD:URL", value = handler function
    unordered_map<string, RouteHandler> get_routes;
    unordered_map<string, RouteHandler> post_routes;
    unordered_map<string, RouteHandler> put_routes;
    unordered_map<string, RouteHandler> delete_routes;

    LOCKER routes_locker; ///< Mutex for thread safety
    std::string doc_root;
    bool static_files = false;

public:
    // Delete copy constructor and assignment operator
    ROUTER(const ROUTER &) = delete;
    ROUTER &operator=(const ROUTER &) = delete;

    // Singleton instance getter
    static ROUTER &get_instance()
    {
        static ROUTER instance;
        return instance;
    }

    void make_static(std::string root)
    {
        // find server path and store it to sting and add /root at last of string and then store it to m_root
        char server_path[200];
        getcwd(server_path, 200);
        doc_root = server_path + root;

        static_files = true;
    }

    bool isStatic()
    {
        return static_files;
    }

    char *root_path()
    {
        return doc_root.data();
    }

    // Add route to the map
    void add_route(const METHOD &method, const string &path, RouteHandler handler)
    {
        auto *routes = route_table(method);
        if (!routes)
        {
            LOG_ERROR("Invalid HTTP method for route: %s", path.c_str());
            return;
        }
        routes_locker.lock();
        (*routes)[path] = handler;
        routes_locker.unlock();
    }
    // Handle incoming request
    void handleRequest(const HttpRequest &req, HttpResponse &res)
    {
        auto *routes = route_table(req.m_method);
        if (routes)
        {
            auto it = routes->find(req.m_url);
            if (it != routes->end())
            {
                RouteHandler handler = it->second;
                handler(req, res);
                return;
            }
        }
        if (res.render(200, req.m_url))
            return;

        res.send(404, "404 not found");
    }
    // Convenience methods for common HTTP methods
    void get(const string &path, RouteHandler handler)
    {
        add_route(GET, path, handler);
    }
    void post(const string &path, RouteHandler handler)
    {
        add_route(POST, path, handler);
    }
    void put(const string &path, RouteHandler handler)
    {
        add_route(PUT, path, handler);
    }
    void del(const string &path, RouteHandler handler)
    {
        add_route(DELETE, path, handler);
    }

    unordered_map<string, RouteHandler> *route_table(METHOD method)
    {
        switch (method)
        {
        case GET:
            return &get_routes;
        case POST:
            return &post_routes;
        case PUT:
            return &put_routes;
        case DELETE:
            return &delete_routes;
        default:
            return nullptr;
        }
    }
};

#endif
