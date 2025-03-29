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
    unordered_map<string, RouteHandler> routes;

    LOCKER routes_locker; ///< Mutex for thread safety

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
    // Add route to the map
    void add_route(const METHOD &method, const string &path, RouteHandler handler)
    {
        string key = find_method_str(method) + ":" + path;
        routes_locker.lock();
        routes[key] = handler;
        routes_locker.unlock();
    }
    // Handle incoming request
    void handleRequest(const HttpRequest &req, HttpResponse &res)
    {
        string key = find_method_str(req.m_method) + ":" + req.m_url;
        routes_locker.lock();
        auto it = routes.find(key);
        if (it != routes.end())
        {
            // Found matching route, execute handler
            // Unlock before calling the handler to avoid holding the lock during user code
            RouteHandler handler = it->second;
            routes_locker.unlock();
            handler(req, res);
            return;
        }
        else if (res.render(202,req.m_url))
        {
            routes_locker.unlock();
            return;
        }
        else
        {
            routes_locker.unlock();
            res.send(404, "404 not found");
        }
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

    string find_method_str(METHOD meth)
    {
        switch (meth)
        {
        case GET:
            return "GET";
            break;

        case POST:
            return "POST";
            break;
        case PUT:
            return "PUT";
            break;
        case HEAD:
            return "HEAD";
            break;

        case DELETE:
            return "DELETE";
            break;

        case TRACE:
            return "DELETE";
            break;

        case OPTIONS:
            return "DELETE";
            break;

        case CONNECT:
            return "CONNECT";
            break;

        case PATH:
            return "PATH";
            break;

        default:
            return "";
            break;
        }
    }
};

#endif