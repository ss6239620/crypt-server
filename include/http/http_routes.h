#pragma once

#include <unordered_map>
#include <functional>
#include <string>
#include <iostream>
#include <unistd.h>

#include "log/log.h"
#include "http_constants.h"
#include "http_request.h"
#include "http_response.h"

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
        // getcwd() gives the process working directory; appending /root creates the static file base path.
        char server_path[1024];
        if (getcwd(server_path, sizeof(server_path)) == nullptr)
        {
            LOG_ERROR("%s", "Failed to resolve static root path");
            return;
        }
        doc_root = server_path + root;

        static_files = true;
    }

    bool isStatic() const
    {
        return static_files;
    }

    const std::string &root_path() const
    {
        return doc_root;
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

        // Routes are registered during startup before worker threads accept traffic.
        (*routes)[path] = handler;
    }
    // Handle incoming request
    void handleRequest(const HttpRequest &req, HttpResponse &res)
    {
        auto *routes = route_table(req.method_type);
        if (routes)
        {
            auto it = routes->find(req.path);
            if (it != routes->end())
            {
                it->second(req, res);
                return;
            }
        }

        // Static fallback is GET-only; POST /missing should be a route miss, not a file lookup.
        if (req.method_type == GET && res.render(200, req.path))
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
