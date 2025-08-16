# Modern C++ HTTP Server

High-performance, memory-efficient HTTP/1.1 server built with modern C++20 and Boost.Asio. Features async I/O, automatic compression, static file serving, WebSocket support, HTTPS/SSL encryption, and extensible middleware pipeline for building web applications and APIs.

![License](https://img.shields.io/badge/License-MIT-blue.svg)
![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20macOS%20%7C%20Windows-lightgrey.svg)

## Features

- **Async I/O** - High throughput, non-blocking architecture built on Boost.Asio
- **HTTP/1.1** - Persistent connections, chunked encoding, standard methods
- **WebSocket** - Full RFC 6455 implementation with real-time bidirectional communication
- **HTTPS/SSL** - TLS encryption with configurable cipher suites and certificate management
- **Rate Limiting** - Advanced traffic control with Token Bucket, Fixed Window, and Sliding Window algorithms
- **Static Files** - Built-in file server with MIME type detection and ETag caching
- **JSON Config** - Flexible runtime configuration
- **Middleware** - Extensible request/response processing pipeline
- **Compression** - Automatic gzip compression for supported content

## Core Components

- **HttpServer** - Main orchestrator, routing, configuration
- **Boost.Asio** - Async I/O event loop
- **Connection** - Per-client connection handling with timeouts
- **HttpRequest** - HTTP/1.1 parser with validation
- **HttpResponse** - Fluent response builder with compression and ETag caching

## Documentation

**[View Full Documentation →](DOCUMENTATION.md)**

- **Quick Start** - Get running in minutes with examples
- **API Reference** - Complete C++ API with code samples
- **Configuration** - JSON config options and programmatic setup
- **Test** - Unit tests, benchmarking, and validation ([test/README.md](test/README.md))
- **Deployment** - Docker, systemd, reverse proxy setup
- **Development** - Project structure and dependencies
- **Limitations** - Limitations on features and protocol support 

---
