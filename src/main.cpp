/**
 * @file main.cpp
 * @brief Entry point for the Modern C++ HTTP Server. Handles configuration, sets up routes and middleware, and starts the server.
 *
 * This file demonstrates how to configure, extend, and launch the HTTP server.
 * It includes example routes, middleware, and signal handling for graceful shutdown.
 */
#include <iostream>
#include <signal.h>
#include <memory>
#include <filesystem>
#include <fstream>
#include "server.hpp"
#include "compression.hpp"

using namespace http_server;

// Global server instance for signal handling
std::unique_ptr<HttpServer> g_server;

/**
 * @brief Signal handler for graceful server shutdown.
 * @param signal The received signal number.
 */
void signal_handler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down server..." << std::endl;
    if (g_server) {
        g_server->stop();
    }
}

/**
 * @brief Register example HTTP routes on the server.
 * @param server Reference to the HttpServer instance.
 */
void setup_routes(HttpServer& server) {
    // Simple GET route
    server.add_get_route("/hello", [](const HttpRequest& /*request*/) {
        return HttpResponse::ok("Hello, World!");
    });
    
    // JSON API endpoint
    server.add_get_route("/api/status", [&server](const HttpRequest& /*request*/) {
        return HttpResponse::json_response(server.stats_json());
    });
    
    // Route with query parameters
    server.add_get_route("/greet", [](const HttpRequest& request) {
        auto name = request.get_query_param("name");
        std::string greeting = "Hello, " + (name ? *name : "Anonymous") + "!";
        return HttpResponse::ok(greeting);
    });
    
    // POST endpoint for data
    server.add_post_route("/api/data", [](const HttpRequest& request) {
        std::string body = request.body();
        if (body.empty()) {
            return HttpResponse::bad_request("Request body is required");
        }
        
        // Echo back the received data
        nlohmann::json response;
        response["received"] = body;
        response["content_type"] = request.content_type();
        response["content_length"] = request.content_length();
        
        return HttpResponse::json_response(response.dump());
    });
    
    // Route with path parameter simulation
    server.add_get_route("/user/*", [](const HttpRequest& request) {
        std::string path = request.path();
        size_t pos = path.find_last_of('/');
        if (pos != std::string::npos && pos < path.length() - 1) {
            std::string user_id = path.substr(pos + 1);
            
            nlohmann::json user_info;
            user_info["id"] = user_id;
            user_info["name"] = "User " + user_id;
            user_info["email"] = user_id + "@example.com";
            
            return HttpResponse::json_response(user_info.dump());
        }
        
        return HttpResponse::bad_request("Invalid user ID");
    });
    
    // HTML response example
    server.add_get_route("/dashboard", [](const HttpRequest& /*request*/) {
        std::string html = R"(
<!DOCTYPE html>
<html>
<head>
    <title>HTTP Server Dashboard</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 40px; }
        .header { color: #333; border-bottom: 2px solid #333; padding-bottom: 10px; }
        .info { background: #f5f5f5; padding: 20px; margin: 20px 0; border-radius: 5px; }
        .endpoint { margin: 10px 0; }
        a { color: #0066cc; text-decoration: none; }
        a:hover { text-decoration: underline; }
    </style>
</head>
<body>
    <h1 class="header">C++ HTTP Server Dashboard</h1>
    
    <div class="info">
        <h2>Available Endpoints</h2>
        <div class="endpoint"><strong>GET</strong> <a href="/hello">/hello</a> - Simple greeting</div>
        <div class="endpoint"><strong>GET</strong> <a href="/api/status">/api/status</a> - Server statistics</div>
        <div class="endpoint"><strong>GET</strong> <a href="/greet?name=YourName">/greet?name=YourName</a> - Personalized greeting</div>
        <div class="endpoint"><strong>GET</strong> <a href="/user/123">/user/{id}</a> - User information</div>
        <div class="endpoint"><strong>POST</strong> /api/data - Echo data back</div>
        <div class="endpoint"><strong>GET</strong> / - Static file serving (if enabled)</div>
    </div>
    
    <div class="info">
        <h2>Server Features</h2>
        <ul>
            <li>High-performance async I/O with Boost.Asio</li>
            <li>Thread pool for request handling</li>
            <li>Static file serving with MIME type detection</li>
            <li>JSON configuration support</li>
            <li>Request/response middleware</li>
            <li>Keep-alive connections</li>
            <li>Comprehensive logging</li>
        </ul>
    </div>
</body>
</html>
        )";
        
        return HttpResponse().set_html(html);
    });
    
    // Large response route for testing compression
    server.add_get_route("/large", [](const HttpRequest& /*request*/) {
        std::string large_content = 
            "This is a large response designed to test compression functionality. ";
        // Repeat the content to make it large enough for compression
        for (int i = 0; i < 100; ++i) {
            large_content += "Lorem ipsum dolor sit amet, consectetur adipiscing elit. "
                           "Sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. "
                           "Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris. ";
        }
        return HttpResponse::ok(large_content).set_content_type("text/plain");
    });
}

/**
 * @brief Register example middleware on the server (CORS, logging, rate limiting).
 * @param server Reference to the HttpServer instance.
 */
void setup_middleware(HttpServer& server) {
    // CORS middleware
    server.add_middleware([](const HttpRequest& request, HttpResponse& response) {
        // Handle preflight requests
        if (request.method() == HttpMethod::OPTIONS) {
            response.set_status(HttpStatus::OK)
                   .set_cors_headers()
                   .set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS")
                   .set_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
            return false; // Stop processing, return response immediately
        }
        
        // Add CORS headers to all responses
        response.set_cors_headers();
        return true; // Continue processing
    });
    
    // Logging middleware
    server.add_middleware([](const HttpRequest& request, HttpResponse& /*response*/) {
        std::cout << "[MIDDLEWARE] " << HttpRequest::method_to_string(request.method()) 
                  << " " << request.path() << std::endl;
        return true;
    });
    
    // Rate limiting simulation (basic example)
    server.add_middleware([](const HttpRequest& request, HttpResponse& response) {
        // In a real implementation, you'd track requests per IP
        // This is just a demonstration
        if (request.path() == "/api/limited") {
            response.set_status(HttpStatus::SERVICE_UNAVAILABLE)
                   .set_text("Rate limit exceeded");
            return false;
        }
        return true;
    });
}

/**
 * @brief Main entry point. Loads configuration, sets up the server, and starts it.
 * @param argc Argument count.
 * @param argv Argument vector.
 * @return int Exit code.
 */
int main(int argc, char* argv[]) {
    try {
        // Setup signal handling
        signal(SIGINT, signal_handler);
        signal(SIGTERM, signal_handler);
        
        // Load configuration
        ServerConfig config;
        std::string config_file = "config/server_config.json";
        
        // Check for custom config file
        if (argc > 1) {
            config_file = argv[1];
        }
        
        // Try to load config file
        if (std::filesystem::exists(config_file)) {
            std::cout << "Loading configuration from: " << config_file << std::endl;
            config = ServerConfig::from_json(config_file);
        } else {
            std::cout << "Configuration file not found, using defaults" << std::endl;
            std::cout << "You can specify a config file: " << argv[0] << " <config.json>" << std::endl;
        }
        
        // Create public directory if it doesn't exist
        if (config.serve_static_files) {
            std::filesystem::create_directories(config.document_root);
            
            // Create a simple index.html if it doesn't exist
            std::string index_path = config.document_root + "/index.html";
            if (!std::filesystem::exists(index_path)) {
                std::ofstream index_file(index_path);
                index_file << R"(<!DOCTYPE html>
<html>
<head>
    <title>C++ HTTP Server</title>
    <style>
        body { font-family: Arial, sans-serif; text-align: center; margin-top: 100px; }
        .container { max-width: 600px; margin: 0 auto; }
        h1 { color: #333; }
        .link { display: inline-block; margin: 10px; padding: 10px 20px; 
                background: #0066cc; color: white; text-decoration: none; 
                border-radius: 5px; }
        .link:hover { background: #0055aa; }
    </style>
</head>
<body>
    <div class="container">
        <h1>Welcome to C++ HTTP Server!</h1>
        <p>A high-performance HTTP server built with modern C++20</p>
        <a href="/dashboard" class="link">View Dashboard</a>
        <a href="/api/status" class="link">Server Status</a>
        <a href="/hello" class="link">Hello World</a>
    </div>
</body>
</html>)";
            }
        }
        
        // Create and configure server
        g_server = std::make_unique<HttpServer>(config);
        
        // Setup routes and middleware
        setup_middleware(*g_server);
        setup_routes(*g_server);
        
        std::cout << "Starting server with configuration:" << std::endl;
        std::cout << "- Host: " << config.host << std::endl;
        std::cout << "- Port: " << config.port << std::endl;
        if (config.enable_https) {
            std::cout << "- HTTPS Port: " << config.https_port << std::endl;
            std::cout << "- SSL Certificate: " << config.ssl_certificate_file << std::endl;
            std::cout << "- SSL Private Key: " << config.ssl_private_key_file << std::endl;
        }
        std::cout << "- Thread pool size: " << config.thread_pool_size << std::endl;
        std::cout << "- Document root: " << config.document_root << std::endl;
        std::cout << "- Static files: " << (config.serve_static_files ? "enabled" : "disabled") << std::endl;
        std::cout << "- HTTPS: " << (config.enable_https ? "enabled" : "disabled") << std::endl;
        std::cout << "\nPress Ctrl+C to stop the server" << std::endl;
        
        // Start the server (blocks until stopped)
        g_server->start();
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}