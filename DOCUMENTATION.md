# HTTP Server Documentation

Detailed documentation for the Modern C++ HTTP Server implementation.

## Table of Contents

- [Installation & Quick Start](#installation--quick-start)
- [API Documentation](#api-documentation)
- [Configuration Reference](#configuration-reference)
- [Testing Guide](#testing-guide)
- [Performance & Benchmarking](#performance--benchmarking)
- [Development Guide](#development-guide)
- [Deployment](#deployment)

## Installation & Quick Start

### Build from Source

```bash
# Clone the repository
git clone https://github.com/l1-ca0/http-server-cpp.git
cd http-server-cpp

# Build with different options
./scripts/build.sh release --clean --tests    # Production build with tests
./scripts/build.sh debug --tests              # Debug build with tests
./scripts/build.sh relwithdebinfo --install   # Optimized with debug info
```

### Build Options

| Option/Type         | Description                        |
|---------------------|------------------------------------|
| debug               | Debug build with sanitizers        |
| release             | Optimized production build         |
| relwithdebinfo      | Optimized with debug info          |
| --clean             | Clean build directory              |
| --tests             | Build and run unit tests           |
| --install           | Install to system                  |
| --verbose           | Verbose output                     |

### Quick Start

```bash
# Start server with default configuration
./build/http_server

# Start with custom configuration
./build/http_server config/server_config.json

# Start with custom options
./scripts/run.sh --port 3000 --host localhost
```

### Simple C++ Example

```cpp
#include "server.hpp"
using namespace http_server;

int main() {
    // Create server with default config
    ServerConfig config;
    config.port = 8080;
    
    // Enable HTTPS support
    config.enable_https = true;
    config.https_port = 8443;
    config.ssl_certificate_path = "certs/server.crt";
    config.ssl_private_key_path = "certs/server.key";
    
    HttpServer server(config);
    
    // Add a simple route
    server.add_get_route("/hello", [](const HttpRequest& req) {
        return HttpResponse::ok("Hello, World!");
    });
    
    // Add JSON API endpoint
    server.add_get_route("/api/users", [](const HttpRequest& req) {
        nlohmann::json users = {
            {"users", {
                {{"id", 1}, {"name", "Alice"}},
                {{"id", 2}, {"name", "Bob"}}
            }}
        };
        return HttpResponse::json_response(users.dump());
    });
    
    // Start server (handles both HTTP and HTTPS)
    server.start(); // Blocks until shutdown
    
    return 0;
}
```

## API Documentation

### HTTP Methods

```cpp
// GET routes
server.add_get_route("/users", handler);
server.add_get_route("/users/*", handler); // Wildcard support

// POST routes
server.add_post_route("/users", handler);

// PUT routes
server.add_put_route("/users/*", handler);

// DELETE routes
server.add_delete_route("/users/*", handler);
```

### Request Handling

```cpp
auto handler = [](const HttpRequest& request) -> HttpResponse {
    // Access request data
    std::string path = request.path();
    HttpMethod method = request.method();
    std::string body = request.body();
    
    // Headers (case-insensitive)
    auto auth = request.get_header("Authorization");
    if (!auth) {
        return HttpResponse::bad_request("Missing authorization header");
    }
    
    // Query parameters
    auto user_id = request.get_query_param("user_id");
    
    // Content helpers
    size_t length = request.content_length();
    std::string type = request.content_type();
    bool keep_alive = request.is_keep_alive();
    
    // Build response
    return HttpResponse::ok("Success")
        .set_header("Content-Type", "application/json")
        .set_cors_headers();
};
```

### Response Building

```cpp
// Simple responses
return HttpResponse::ok("Hello");
return HttpResponse::not_found();
return HttpResponse::bad_request("Invalid data");
return HttpResponse::internal_error();

// JSON responses
return HttpResponse::json_response(json_data);

// File responses
return HttpResponse::file_response("./public/index.html");

// Custom responses
return HttpResponse(HttpStatus::CREATED)
    .set_json(data)
    .set_header("Location", "/users/123")
    .set_cors_headers()
    .set_cache_control("no-cache");
```

### Middleware System

```cpp
// CORS middleware
server.add_middleware([](const HttpRequest& req, HttpResponse& res) {
    res.set_cors_headers();
    return true; // Continue processing
});

// Authentication middleware
server.add_middleware([](const HttpRequest& req, HttpResponse& res) {
    if (!req.has_header("Authorization")) {
        res.set_status(HttpStatus::BAD_REQUEST)
           .set_text("Missing authorization");
        return false; // Stop processing
    }
    return true;
});

// Logging middleware
server.add_middleware([](const HttpRequest& req, HttpResponse& res) {
    std::cout << req.method_to_string(req.method()) 
              << " " << req.path() << std::endl;
    return true;
});
```

### Compression Support

The server automatically compresses responses when:

- Client sends `Accept-Encoding: gzip` header
- Response body is larger than 1024 bytes (configurable)
- Content type is compressible (text/*, application/json, etc.)

```cpp
// Manual compression (if needed)
HttpResponse response;
response.set_compressed_body("Large response data", "gzip");

// Check if response is compressed
if (response.is_compressed()) {
    // Handle compressed response
}
```

## Configuration Reference

### JSON Configuration File

```json
{
  "host": "0.0.0.0",
  "port": 8080,
  "enable_https": true,
  "https_port": 8443,
  "ssl_certificate_path": "certs/server.crt",
  "ssl_private_key_path": "certs/server.key",
  "ssl_ciphers": "ECDHE-RSA-AES256-GCM-SHA384:ECDHE-RSA-AES128-GCM-SHA256",
  "ssl_verify_client": false,
  "thread_pool_size": 4,
  "document_root": "./public",
  "max_connections": 1000,
  "keep_alive_timeout": 30,
  "max_request_size": 1048576,
  "enable_logging": true,
  "log_file": "server.log",
  "serve_static_files": true,
  "index_files": [
    "index.html",
    "index.htm",
    "default.html"
  ],
  "enable_compression": true,
  "compression_min_size": 1024,
  "compression_level": 6,
  "compressible_types": [
    "text/plain",
    "text/html", 
    "text/css",
    "application/javascript",
    "application/json",
    "application/xml",
    "text/xml"
  ],
  "mime_types": {
    "html": "text/html; charset=utf-8",
    "css": "text/css",
    "js": "application/javascript",
    "json": "application/json",
    "png": "image/png",
    "jpg": "image/jpeg"
  }
}
```

### Programmatic Configuration

```cpp
ServerConfig config;
config.host = "127.0.0.1";
config.port = 8080;

// HTTPS Configuration
config.enable_https = true;
config.https_port = 8443;
config.ssl_certificate_path = "certs/server.crt";
config.ssl_private_key_path = "certs/server.key";
config.ssl_ciphers = "ECDHE-RSA-AES256-GCM-SHA384:ECDHE-RSA-AES128-GCM-SHA256";
config.ssl_verify_client = false;

config.thread_pool_size = 4;
config.document_root = "./public";
config.serve_static_files = true;
config.keep_alive_timeout = std::chrono::seconds(30);
config.enable_compression = true;
config.compression_min_size = 1024;
config.compression_level = 6;

HttpServer server(config);
```

### Configuration Options

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| host | string | "0.0.0.0" | Server bind address |
| port | int | 8080 | HTTP server port |
| enable_https | bool | false | Enable HTTPS/SSL support |
| https_port | int | 8443 | HTTPS server port |
| ssl_certificate_path | string | "" | Path to SSL certificate file (.crt or .pem) |
| ssl_private_key_path | string | "" | Path to SSL private key file (.key or .pem) |
| ssl_ciphers | string | Modern cipher suite | SSL/TLS cipher configuration |
| ssl_verify_client | bool | false | Require client certificate verification |
| thread_pool_size | int | 4 | Thread pool size (currently unused) |
| document_root | string | "./public" | Static files directory |
| max_connections | int | 1000 | Maximum concurrent connections |
| keep_alive_timeout | int | 30 | Keep-alive timeout in seconds |
| max_request_size | int | 1048576 | Maximum request size in bytes |
| enable_logging | bool | true | Enable request logging |
| log_file | string | "server.log" | Log file path |
| serve_static_files | bool | true | Enable static file serving |
| enable_compression | bool | true | Enable gzip compression |
| compression_min_size | int | 1024 | Minimum size for compression |
| compression_level | int | 6 | Compression level (1-9) |

## Testing Guide

### Test Suites

| Suite             | Description           | Coverage                                                                 |
|-------------------|----------------------|--------------------------------------------------------------------------|
| HttpServerTest    | Core functionality   | Config loading, route/middleware registration, server stats |
| HttpRequestTest   | Request parsing      | HTTP methods, headers, query params, keep-alive, request validation      |
| HttpResponseTest  | Response generation  | Status codes, headers, body, MIME types, compression, serialization     |
| SecurityTest      | Security validation  | Path traversal, request size, header injection, security headers         |
| PerformanceTest   | Performance testing  | Concurrency, memory usage, stress, rapid config, malformed requests      |
| HttpProtocolTest  | HTTP/1.1 compliance  | Chunked encoding, compression, protocol edge cases                       |
| HttpsServerTest   | HTTPS/SSL testing    | SSL context, certificates, encrypted connections, HTTPS configuration    |

### Running Tests

```bash
# Build and run all tests
./scripts/build.sh debug --tests

# Run specific test suite
./build/test_runner --gtest_filter="HttpServerTest.*"

# Run HTTPS tests
./build/test_runner --gtest_filter="HttpsServerTest.*"

# Run SSL-specific tests  
./build/test_runner --gtest_filter="*Ssl*"

# Run with detailed output
./build/test_runner --gtest_print_time=1

# Run tests with verbose output
./build/test_runner --gtest_filter="*" --gtest_print_time=1

# Memory leak detection (if built with sanitizers)
./scripts/build.sh debug --tests
# Tests automatically run with AddressSanitizer and UBSan
```

### Manual Testing

```bash
# Start server
./build/http_server

# Test basic functionality
curl http://localhost:8080/
curl http://localhost:8080/api/status

# Test HTTPS functionality  
curl -k https://localhost:8443/
curl -k https://localhost:8443/api/status

# Test with headers
curl -H "Accept-Encoding: gzip" http://localhost:8080/
curl -k -H "Accept-Encoding: gzip" https://localhost:8443/

# Test POST requests
curl -X POST -d "test data" http://localhost:8080/api/echo

# Test static files
curl http://localhost:8080/index.html
```

## Performance & Benchmarking

### Built-in Benchmarking

```bash
# Built-in benchmark script with wrk
./scripts/benchmark.sh --tool wrk --duration 30s --connections 100

# Apache Bench
./scripts/benchmark.sh --tool ab --requests 10000 --concurrency 100

# Custom scenarios
./scripts/benchmark.sh --custom --scenario static_files
./scripts/benchmark.sh --custom --scenario json_api
```

### Manual Benchmarking

```bash
# Using wrk
wrk -t12 -c400 -d30s http://localhost:8080/

# Using Apache Bench
ab -n 10000 -c 100 http://localhost:8080/

# Using curl for simple tests
for i in {1..1000}; do curl -s http://localhost:8080/ > /dev/null; done
```

### Performance Monitoring

```bash
# Server statistics endpoint
curl http://localhost:8080/api/status

# Monitor with htop/top
htop -p $(pgrep http_server)

# Memory usage
ps -o pid,vsz,rss,comm -p $(pgrep http_server)
```

## Development Guide

### Project Structure

```text
cpp-http-server/
├── include/         # Public header files
│   ├── server.hpp
│   ├── connection.hpp
│   ├── ssl_connection.hpp
│   ├── request.hpp
│   ├── response.hpp
│   ├── thread_pool.hpp
│   └── compression.hpp
├── src/             # Source implementations
│   ├── main.cpp
│   ├── server.cpp
│   ├── connection.cpp
│   ├── ssl_connection.cpp
│   ├── request.cpp
│   ├── response.cpp
│   ├── thread_pool.cpp
│   └── compression.cpp
├── test/            # Unit and protocol tests
│   ├── test_server.cpp
│   ├── test_request.cpp
│   ├── test_response.cpp
│   ├── test_security.cpp
│   ├── test_performance.cpp
│   ├── test_http_protocol.cpp
│   └── test_https.cpp
├── config/          # JSON configuration files
├── public/          # Static files for serving
├── certs/           # SSL certificates for HTTPS
├── scripts/         # Build, run, and benchmark scripts
├── docker/          # Docker and container configs
└── CMakeLists.txt   # CMake build configuration
```

### Dependencies

- **Boost.Asio** - Asynchronous I/O operations
- **OpenSSL** - SSL/TLS support for HTTPS functionality
- **nlohmann/json** - JSON configuration and responses  
- **Google Test** - Unit testing framework
- **ZLIB** - Gzip compression support

All dependencies are automatically fetched and configured by CMake.

### Build System

The project uses modern CMake (3.20+) with:

- C++20 standard requirement
- Automatic dependency management with FetchContent
- Platform-specific optimizations
- Debug builds with sanitizers
- Comprehensive compiler warnings

### Code Style

- Modern C++20 features (concepts, ranges, coroutines where applicable)
- RAII and smart pointers
- Const-correctness
- Exception safety
- Clear naming conventions

## Deployment

### SSL Certificate Setup

For HTTPS deployment, you'll need SSL certificates:

```bash
# Generate self-signed certificates for development/testing
openssl req -x509 -newkey rsa:4096 -keyout server.key -out server.crt -days 365 -nodes

# Or use Let's Encrypt for production
certbot certonly --standalone -d your-domain.com

# Configure paths in server config
{
  "enable_https": true,
  "https_port": 8443,
  "ssl_certificate_path": "/path/to/server.crt",
  "ssl_private_key_path": "/path/to/server.key",
  "ssl_ciphers": "ECDHE-RSA-AES256-GCM-SHA384:ECDHE-RSA-AES128-GCM-SHA256"
}
```

### Docker Support

```bash
# Build Docker image
docker build -t cpp-http-server .

# Run with Docker
docker run -p 8080:8080 cpp-http-server

# Use Docker Compose
docker-compose up

# Production deployment with Nginx reverse proxy
docker-compose -f docker-compose.prod.yml up
```

### Systemd Service

```ini
[Unit]
Description=C++ HTTP Server
After=network.target

[Service]
Type=simple
User=www-data
WorkingDirectory=/opt/cpp-http-server
ExecStart=/opt/cpp-http-server/http_server config/server_config.json
Restart=always
RestartSec=10

[Install]
WantedBy=multi-user.target
```

### Reverse Proxy Setup

Example Nginx configuration for HTTP and HTTPS:

```nginx
# HTTP server (redirect to HTTPS)
server {
    listen 80;
    server_name example.com;
    return 301 https://$server_name$request_uri;
}

# HTTPS server (proxy to backend)
server {
    listen 443 ssl http2;
    server_name example.com;
    
    # SSL configuration (if terminating SSL at proxy)
    ssl_certificate /path/to/ssl/cert.pem;
    ssl_certificate_key /path/to/ssl/private.key;
    ssl_protocols TLSv1.2 TLSv1.3;
    ssl_ciphers ECDHE-RSA-AES128-GCM-SHA256:ECDHE-RSA-AES256-GCM-SHA384;
    
    location / {
        # Proxy to HTTP backend (SSL termination at proxy)
        proxy_pass http://127.0.0.1:8080;
        
        # Or proxy to HTTPS backend (end-to-end encryption)
        # proxy_pass https://127.0.0.1:8443;
        
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;
    }
}
```

### Monitoring

The server includes built-in statistics:

```bash
# Get server status
curl http://localhost:8080/api/status
```

For production monitoring, consider:

- Prometheus metrics collection
- Log aggregation with ELK stack
- Health check endpoints
- Resource monitoring with tools like htop, iostat

### Security Considerations

- Configure HTTPS with proper SSL certificates for production
- Run behind reverse proxy for additional security layers
- Configure firewall rules
- Set appropriate file permissions for SSL certificates
- Use non-root user for service
- Implement rate limiting at proxy level
- Regular security updates
- Use strong cipher suites and disable weak protocols

## Limitations

This server has limitations compared to production servers:

### Protocol Support

- **HTTP/1.1 Only** - No HTTP/2 or HTTP/3 support  
- **No WebSockets** - Real-time communication not supported

### HTTP/1.1 Feature Gaps

- **No Caching Support** - Missing ETag and conditional requests
- **No Range Requests** - Partial content delivery not implemented
- **Basic Authentication** - No built-in auth schemes

### HTTPS/SSL Limitations

- **No HTTP/2 over TLS** - HTTPS uses HTTP/1.1 only
- **No Advanced SSL Features** - Missing HSTS, certificate pinning, OCSP stapling
- **Self-Signed Certificates** - Production deployments need proper CA-signed certificates

For production use, consider deploying behind a full-featured reverse proxy like Nginx or Apache for advanced features.
