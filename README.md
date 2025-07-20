# Modern C++ HTTP Server

High-performance, memory-efficient HTTP/1.1 server built with modern C++20 and Boost.Asio. Features async I/O, automatic compression, static file serving, and extensible middleware pipeline for building web applications and APIs.

![License](https://img.shields.io/badge/License-MIT-blue.svg)
![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20macOS%20%7C%20Windows-lightgrey.svg)

## Features

- **Async I/O** - High throughput, non-blocking architecture built on Boost.Asio
- **HTTP/1.1** - Persistent connections, chunked encoding, standard methods
- **Static Files** - Built-in file server with MIME type detection
- **JSON Config** - Flexible runtime configuration
- **Middleware** - Extensible request/response processing pipeline
- **Compression** - Automatic gzip compression for supported content

## Core Components

- **HttpServer** - Main orchestrator, routing, configuration
- **Boost.Asio** - Async I/O event loop
- **Connection** - Per-client connection handling with timeouts
- **HttpRequest** - HTTP/1.1 parser with validation
- **HttpResponse** - Fluent response builder with compression

---
