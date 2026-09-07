# crypt-server

`crypt-server` is a C++ HTTP web server built around Linux networking primitives. It is a small systems-programming project that demonstrates how to accept TCP connections, parse HTTP requests, serve static files, dispatch work to a thread pool, manage idle connection timeouts, and reuse MySQL connections through a connection pool.

The project currently behaves like a lightweight demo web server. `main.cpp` registers a few example routes, enables static file serving from `root/`, initializes logging, creates the MySQL pool, starts worker threads, and then runs the epoll event loop.

## What It Includes

- Event-driven networking with `epoll`
- Level-triggered and edge-triggered connection modes
- Reactor and Proactor-style request handling options
- Fixed-size pthread-based thread pool
- HTTP/1.1 request parsing for `GET` and `POST`
- Static file responses using `mmap` and `writev`
- Basic router for registering `GET`, `POST`, `PUT`, and `DELETE` handlers
- Synchronous or asynchronous logging with daily/log-size rotation
- Sorted timer list for closing inactive client connections
- MySQL connection pool with RAII-style checkout/release
- Sample HTML pages in `root/`

## Project Structure

```text
.
|-- main.cpp                  # Entry point and example route registration
|-- makefile                  # Build target for the server binary
|-- build.sh                  # Small wrapper around make
|-- config/                   # Runtime configuration and CLI flag parsing
|-- webserver/                # Main server lifecycle and epoll loop
|-- http/                     # HTTP connection, routing, response, and JSON code
|-- threadpool/               # Worker pool implementation
|-- timer/                    # Connection timeout timer list and signal helpers
|-- log/                      # Thread-safe sync/async logger
|-- cgi_mysql/                # MySQL connection pool
|-- lock/                     # Mutex, semaphore, and condition wrappers
|-- root/                     # Static HTML/media files served by the server
`-- test/                     # Python request script for manual testing
```

## Requirements

This server uses Linux-specific APIs, so it is intended to run on Linux.

- `g++`
- `make`
- POSIX threads
- MySQL client development headers/library, providing `mysql/mysql.h` and `libmysqlclient`
- Python 3 with `requests`, only if you want to run `test/testing.py`

On Debian/Ubuntu-like systems, the native dependencies are typically installed with packages similar to:

```bash
sudo apt install build-essential default-libmysqlclient-dev
```

## Build

```bash
make
```

or:

```bash
./build.sh
```

The build creates a `server` executable in the project root.

To remove the executable:

```bash
make clean
```

## Run

```bash
./server
```

By default, the server listens on port `9906`.

Open the server in a browser:

```text
http://localhost:9906/
```

The default request path is mapped to the sample static page at `root/judge.html`.

## Runtime Options

Configuration is passed through command-line flags:

```text
-p <port>       Listening port, default 9906
-l <0|1>        Log mode: 0 sync, 1 async
-m <0|1|2|3>    Trigger mode:
                0 LT + LT
                1 LT + ET
                2 ET + LT
                3 ET + ET
-o <0|1>        Socket linger option
-s <number>     MySQL connection pool size, default 8
-t <number>     Worker thread count, default 8
-c <0|1>        Logging: 0 enable, 1 disable
-a <0|1>        Actor model: 0 Proactor, 1 Reactor
```

Example:

```bash
./server -p 8080 -t 16 -s 8 -l 1 -m 3
```

## Example Routes

The sample routes are registered in `main.cpp`:

```text
GET  /          Plain text root handler, although / is currently normalized to /judge.html
GET  /about     Plain text about handler
POST /login     Plain text login demo handler
GET  /contact   Renders /video.html from the static root
```

Static files under `root/` can also be requested directly, for example:

```text
http://localhost:9906/judge.html
http://localhost:9906/register.html
http://localhost:9906/video.html
```

## Testing

After starting the server, you can run the Python test script:

```bash
python3 test/testing.py
```

The script sends basic `GET`, `POST`, error-path, and keep-alive requests to `localhost:9906`.

## Notes

- The current `main.cpp` contains sample database credentials and a fixed MySQL host. Move these values into environment variables or a local config file before using the server outside a private development environment.
- The HTML files include login/register forms, and the codebase contains MySQL connection-pool plumbing, but the current route setup is mainly a server demo rather than a complete authentication application.
- Because the networking layer depends on `epoll`, this project is not portable to Windows or macOS without replacing the event backend.
