#include <iostream>
#include <unordered_map>
#include <functional>
#include <string>

#include "../lock/locker.h"
// Forward declaration
class HttpRequest;
class HttpResponse;

// Alias for handler function type
using RouteHandler = std::function<void(const HttpRequest &, HttpResponse &)>;

// Simple request and response classes (simplified for example)
class HttpRequest
{
public:
    std::string method;
    std::string url;
    // Add more request properties as needed...
};

class HttpResponse
{
public:
    void send(const std::string &content)
    {
        std::cout << "Sending response: " << content << std::endl;
    }
    // Add more response methods as needed...
};

class Router
{
private:
    // Private constructor for singleton
    Router() = default;

    // Map to store routes: key = "METHOD:URL", value = handler function
    std::unordered_map<std::string, RouteHandler> routes;

    // Mutex for thread safety
    LOCKER routesLocker;

public:
    // Delete copy constructor and assignment operator
    Router(const Router &) = delete;
    Router &operator=(const Router &) = delete;

    // Singleton instance getter
    static Router &getInstance()
    {
        static Router instance;
        return instance;
    }

    // Add route to the map
    void addRoute(const std::string &method, const std::string &path, RouteHandler handler)
    {
        routesLocker.lock();
        std::string key = method + ":" + path;
        routes[key] = handler;
        routesLocker.unlock();
    }

    // Handle incoming request
    void handleRequest(const HttpRequest &req, HttpResponse &res)
    {
        std::string key = req.method + ":" + req.url;

        routesLocker.lock();
        auto it = routes.find(key);
        if (it != routes.end())
        {
            // Found matching route, execute handler
            // Unlock before calling the handler to avoid holding the lock during user code
            RouteHandler handler = it->second;
            routesLocker.unlock();
            handler(req, res);
        }
        else
        {
            // No matching route found
            routesLocker.unlock();
            res.send("404 Not Found");
        }
    }

    // Convenience methods for common HTTP methods
    void get(const std::string &path, RouteHandler handler)
    {
        addRoute("GET", path, handler);
    }

    void post(const std::string &path, RouteHandler handler)
    {
        addRoute("POST", path, handler);
    }

    void put(const std::string &path, RouteHandler handler)
    {
        addRoute("PUT", path, handler);
    }

    void del(const std::string &path, RouteHandler handler)
    {
        addRoute("DELETE", path, handler);
    }
};

// Example usage
int main()
{
    // Get router instance
    Router &router = Router::getInstance();

    // Register routes
    router.get("/", [](const HttpRequest &req, HttpResponse &res)
               { res.send("Hello from root!"); });

    router.get("/about", [](const HttpRequest &req, HttpResponse &res)
               { res.send("About page"); });

    router.post("/login", [](const HttpRequest &req, HttpResponse &res)
                { res.send("Login endpoint"); });

    // Simulate incoming requests
    HttpRequest req1;
    req1.method = "GET";
    req1.url = "/";
    HttpResponse res1;
    router.handleRequest(req1, res1);

    HttpRequest req2;
    req2.method = "GET";
    req2.url = "/about";
    HttpResponse res2;
    router.handleRequest(req2, res2);

    HttpRequest req3;
    req3.method = "POST";
    req3.url = "/login";
    HttpResponse res3;
    router.handleRequest(req3, res3);

    HttpRequest req4;
    req4.method = "GET";
    req4.url = "/nonexistent";
    HttpResponse res4;
    router.handleRequest(req4, res4);

    return 0;
}