# webserv

A minimal C++17 foundation for a 42 School-style web server project.

## Architecture

- `EventLoop` owns the main event loop and monitors sockets.
- `Server` represents one configured virtual server.
- `Client` tracks a connected client and its buffers.
- `Request` models parsed HTTP requests.
- `Response` builds HTTP response data.
- `HttpParser` parses raw requests into `Request` objects.
- `ConfigParser` parses nginx-like server configuration.
- `CGIHandler` provides a stub interface for CGI execution.
- `Logger` offers lightweight logging.
- `Utils` contains shared helper functions.
- `Types` contains shared enums and type aliases.

## Build

Using CMake:

```sh
mkdir -p build
cd build
cmake ..
cmake --build .
```

Using Makefile:

```shn
make
```

## Notes

This repository is a clean skeleton with minimal classes and empty implementations.
