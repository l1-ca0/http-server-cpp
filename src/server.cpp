/**
 * @file server.cpp
 * @brief Implementation of the Modern C++ HTTP Server core logic, including server lifecycle, configuration, routing, and statistics.
 *
 * This file contains the implementation of the HttpServer class, the ServerConfig struct, and the ThreadPool utility for request handling.
 */
#include "server.hpp"
#include "compression.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <thread>
#include <nlohmann/json.hpp>

namespace http_server {

// ThreadPool implementation
ThreadPool::ThreadPool(size_t thread_count) {
    for (size_t i = 0; i < thread_count; ++i) {
        workers_.emplace_back([this] {
            while (true) {
                std::function<void()> task;
                
                {
                    std::unique_lock<std::mutex> lock(queue_mutex_);
                    condition_.wait(lock, [this] { return stop_.load() || !tasks_.empty(); });
                    
                    if (stop_.load() && tasks_.empty()) {
                        return;
                    }
                    
                    task = std::move(tasks_.front());
                    tasks_.pop();
                }
                
                task();
            }
        });
    }
}

ThreadPool::~ThreadPool() {
    shutdown();
}

void ThreadPool::shutdown() {
    stop_.store(true);
    condition_.notify_all();
    
    for (std::thread& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    
    workers_.clear();
}

size_t ThreadPool::pending_tasks() const {
    std::unique_lock<std::mutex> lock(queue_mutex_);
    return tasks_.size();
}

// ServerConfig implementation
ServerConfig ServerConfig::from_json(const std::string& config_file) {
    std::ifstream file(config_file);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open config file: " + config_file);
    }
    
    nlohmann::json json;
    file >> json;
    
    return from_json_string(json.dump());
}

ServerConfig ServerConfig::from_json_string(const std::string& json_str) {
    ServerConfig config;
    auto json = nlohmann::json::parse(json_str);
    
    if (json.contains("host")) config.host = json["host"];
    if (json.contains("port")) config.port = json["port"];
    if (json.contains("thread_pool_size")) config.thread_pool_size = json["thread_pool_size"];
    if (json.contains("document_root")) config.document_root = json["document_root"];
    if (json.contains("max_connections")) config.max_connections = json["max_connections"];
    if (json.contains("keep_alive_timeout")) config.keep_alive_timeout = std::chrono::seconds(json["keep_alive_timeout"]);
    if (json.contains("max_request_size")) config.max_request_size = json["max_request_size"];
    if (json.contains("enable_logging")) config.enable_logging = json["enable_logging"];
    if (json.contains("log_file")) config.log_file = json["log_file"];
    if (json.contains("serve_static_files")) config.serve_static_files = json["serve_static_files"];
    
    if (json.contains("index_files")) {
        config.index_files.clear();
        for (const auto& file : json["index_files"]) {
            config.index_files.push_back(file);
        }
    }
    
    // Compression configuration
    if (json.contains("enable_compression")) config.enable_compression = json["enable_compression"];
    if (json.contains("compression_min_size")) config.compression_min_size = json["compression_min_size"];
    if (json.contains("compression_level")) config.compression_level = json["compression_level"];
    
    if (json.contains("compressible_types")) {
        config.compressible_types.clear();
        for (const auto& type : json["compressible_types"]) {
            config.compressible_types.push_back(type);
        }
    }
    
    if (json.contains("mime_types")) {
        for (const auto& [ext, mime] : json["mime_types"].items()) {
            config.mime_types[ext] = mime;
        }
    }
    
    // HTTPS configuration
    if (json.contains("enable_https")) config.enable_https = json["enable_https"];
    if (json.contains("https_port")) config.https_port = json["https_port"];
    if (json.contains("ssl_certificate_file")) config.ssl_certificate_file = json["ssl_certificate_file"];
    if (json.contains("ssl_private_key_file")) config.ssl_private_key_file = json["ssl_private_key_file"];
    if (json.contains("ssl_ca_file")) config.ssl_ca_file = json["ssl_ca_file"];
    if (json.contains("ssl_dh_file")) config.ssl_dh_file = json["ssl_dh_file"];
    if (json.contains("ssl_verify_client")) config.ssl_verify_client = json["ssl_verify_client"];
    if (json.contains("ssl_cipher_list")) config.ssl_cipher_list = json["ssl_cipher_list"];
    
    return config;
}

nlohmann::json ServerConfig::to_json() const {
    nlohmann::json json;
    
    json["host"] = host;
    json["port"] = port;
    json["thread_pool_size"] = thread_pool_size;
    json["document_root"] = document_root;
    json["max_connections"] = max_connections;
    json["keep_alive_timeout"] = keep_alive_timeout.count();
    json["max_request_size"] = max_request_size;
    json["enable_logging"] = enable_logging;
    json["log_file"] = log_file;
    json["serve_static_files"] = serve_static_files;
    json["index_files"] = index_files;
    json["enable_compression"] = enable_compression;
    json["compression_min_size"] = compression_min_size;
    json["compression_level"] = compression_level;
    json["compressible_types"] = compressible_types;
    json["mime_types"] = mime_types;
    
    // HTTPS configuration
    json["enable_https"] = enable_https;
    json["https_port"] = https_port;
    json["ssl_certificate_file"] = ssl_certificate_file;
    json["ssl_private_key_file"] = ssl_private_key_file;
    json["ssl_ca_file"] = ssl_ca_file;
    json["ssl_dh_file"] = ssl_dh_file;
    json["ssl_verify_client"] = ssl_verify_client;
    json["ssl_cipher_list"] = ssl_cipher_list;
    
    return json;
}

// HttpServer implementation
HttpServer::HttpServer(const ServerConfig& config)
    : config_(config)
    , acceptor_(io_context_)
    , thread_pool_(std::make_unique<ThreadPool>(config_.thread_pool_size)) {
    
    // Initialize HTTPS if enabled
    if (config_.enable_https) {
        https_acceptor_ = std::make_unique<boost::asio::ip::tcp::acceptor>(io_context_);
        ssl_context_ = std::make_unique<boost::asio::ssl::context>(boost::asio::ssl::context::tlsv12);
        initialize_ssl_context();
    }
    
    initialize_mime_types();
    stats_.start_time = std::chrono::steady_clock::now();
}

HttpServer::~HttpServer() {
    stop();
}

void HttpServer::start() {
    if (running_.load()) {
        return;
    }
    
    try {
        // Setup HTTP endpoint
        boost::asio::ip::tcp::endpoint endpoint(
            boost::asio::ip::address::from_string(config_.host),
            config_.port
        );
        
        acceptor_.open(endpoint.protocol());
        acceptor_.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
        acceptor_.bind(endpoint);
        acceptor_.listen();
        
        std::cout << "HTTP Server starting on " << config_.host << ":" << config_.port << std::endl;
        
        // Setup HTTPS endpoint if enabled
        if (config_.enable_https && https_acceptor_) {
            boost::asio::ip::tcp::endpoint https_endpoint(
                boost::asio::ip::address::from_string(config_.host),
                config_.https_port
            );
            
            https_acceptor_->open(https_endpoint.protocol());
            https_acceptor_->set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
            https_acceptor_->bind(https_endpoint);
            https_acceptor_->listen();
            
            std::cout << "HTTPS Server starting on " << config_.host << ":" << config_.https_port << std::endl;
        }
        
        running_.store(true);
        
        std::cout << "Document root: " << config_.document_root << std::endl;
        std::cout << "Thread pool size: " << config_.thread_pool_size << std::endl;
        
        accept_connections();
        if (config_.enable_https && https_acceptor_) {
            accept_ssl_connections();
        }
        
        // Run the io_context in a separate thread
        auto io_thread = std::thread([this]() {
            io_context_.run();
        });
        
        io_thread.join();
        
    } catch (const std::exception& e) {
        std::cerr << "Server start error: " << e.what() << std::endl;
        running_.store(false);
        throw;
    }
}

void HttpServer::stop() {
    if (!running_.load()) {
        return;
    }
    
    running_.store(false);
    
    boost::system::error_code ec;
    acceptor_.close(ec);
    io_context_.stop();
    
    thread_pool_->shutdown();
    
    std::cout << "HTTP Server stopped" << std::endl;
}

void HttpServer::add_route(const std::string& path, HttpMethod method, RequestHandler handler) {
    routes_[{path, method}] = std::move(handler);
}

void HttpServer::add_get_route(const std::string& path, RequestHandler handler) {
    add_route(path, HttpMethod::GET, std::move(handler));
}

void HttpServer::add_post_route(const std::string& path, RequestHandler handler) {
    add_route(path, HttpMethod::POST, std::move(handler));
}

void HttpServer::add_put_route(const std::string& path, RequestHandler handler) {
    add_route(path, HttpMethod::PUT, std::move(handler));
}

void HttpServer::add_delete_route(const std::string& path, RequestHandler handler) {
    add_route(path, HttpMethod::DELETE, std::move(handler));
}

void HttpServer::add_patch_route(const std::string& path, RequestHandler handler) {
    add_route(path, HttpMethod::PATCH, std::move(handler));
}

void HttpServer::add_websocket_route(const std::string& path, WebSocketHandler handler) {
    websocket_routes_[path] = std::move(handler);
}

void HttpServer::add_middleware(MiddlewareHandler middleware) {
    middleware_.push_back(std::move(middleware));
}

void HttpServer::enable_static_files(const std::string& document_root) {
    config_.serve_static_files = true;
    config_.document_root = document_root;
}

void HttpServer::disable_static_files() {
    config_.serve_static_files = false;
}

void HttpServer::update_config(const ServerConfig& new_config) {
    config_ = new_config;
}

std::string HttpServer::stats_json() const {
    nlohmann::json json;
    
    json["total_requests"] = stats_.total_requests.load();
    json["active_connections"] = stats_.active_connections.load();
    json["total_connections"] = stats_.total_connections.load();
    json["bytes_sent"] = stats_.bytes_sent.load();
    json["bytes_received"] = stats_.bytes_received.load();
    
    auto uptime = std::chrono::steady_clock::now() - stats_.start_time;
    auto uptime_seconds = std::chrono::duration_cast<std::chrono::seconds>(uptime).count();
    json["uptime_seconds"] = uptime_seconds;
    
    return json.dump(2);
}

void HttpServer::accept_connections() {
    auto socket = std::make_shared<boost::asio::ip::tcp::socket>(io_context_);
    
    acceptor_.async_accept(*socket,
        [this, socket](const boost::system::error_code& error) {
            handle_accept(error, std::move(*socket));
        }
    );
}

void HttpServer::handle_accept(const boost::system::error_code& error, 
                              boost::asio::ip::tcp::socket socket) {
    if (!error && running_.load()) {
        stats_.total_connections.fetch_add(1);
        stats_.active_connections.fetch_add(1);
        
        auto connection = std::make_shared<Connection>(
            std::move(socket),
            [this](const HttpRequest& request) {
                stats_.total_requests.fetch_add(1);
                
                // Check for WebSocket upgrade first
                if (WebSocketUtils::is_websocket_request(request)) {
                    // Handle WebSocket upgrade (this will be handled in connection level)
                    return handle_websocket_upgrade_response(request);
                }
                
                auto response = handle_request(request);
                log_request(request, response);
                return response;
            },
            [this]() {
                stats_.active_connections.fetch_sub(1);
            }
        );
        
        connection->start();
        
        // Accept next connection
        accept_connections();
    } else if (error) {
        std::cerr << "Accept error: " << error.message() << std::endl;
    }
}

HttpResponse HttpServer::handle_request(const HttpRequest& request) {
    try {
        // Run middleware
        HttpResponse middleware_response;
        for (const auto& middleware : middleware_) {
            if (!middleware(request, middleware_response)) {
                return middleware_response;
            }
        }
        
        HttpResponse response;
        
        // Check for exact route match
        RouteKey key{request.path(), request.method()};
        auto route_it = routes_.find(key);
        
        if (route_it != routes_.end()) {
            response = route_it->second(request);
        } else {
            // Check for pattern matches
            bool route_found = false;
            for (const auto& [route_key, handler] : routes_) {
                if (route_key.method == request.method() && 
                    path_matches(route_key.path, request.path())) {
                    response = handler(request);
                    route_found = true;
                    break;
                }
            }
            
            if (!route_found) {
                // Try static file serving
                if (config_.serve_static_files && request.method() == HttpMethod::GET) {
                    response = handle_static_file(request);
                } else {
                    response = create_error_response(HttpStatus::NOT_FOUND, "Resource not found");
                }
            }
        }
        
        // Apply compression if enabled and supported by client
        if (config_.enable_compression) {
            auto accept_encoding = request.get_header("Accept-Encoding");
            if (accept_encoding) {
                response.compress_body_if_supported(*accept_encoding);
            }
        }
        
        return response;
        
    } catch (const std::exception& e) {
        return create_error_response(HttpStatus::INTERNAL_SERVER_ERROR, 
                                   "Internal server error: " + std::string(e.what()));
    }
}

HttpResponse HttpServer::handle_websocket_upgrade_response(const HttpRequest& request) {
    // Find matching WebSocket route
    for (const auto& [path, handler] : websocket_routes_) {
        if (path_matches(path, request.path())) {
            // Return WebSocket upgrade response
            return WebSocketUtils::create_handshake_response(request);
        }
    }
    
    // No matching WebSocket route found
    return WebSocketUtils::create_handshake_rejection("No WebSocket route found for path: " + request.path());
}

HttpResponse HttpServer::handle_static_file(const HttpRequest& request) {
    std::filesystem::path requested_path = std::filesystem::path(config_.document_root) / request.path().substr(1);
    
    // Security check: ensure path is within document root
    auto canonical_doc_root = std::filesystem::canonical(config_.document_root);
    auto canonical_requested = std::filesystem::weakly_canonical(requested_path);
    
    if (!canonical_requested.string().starts_with(canonical_doc_root.string())) {
        return create_error_response(HttpStatus::FORBIDDEN, "Access denied");
    }
    
    // Check if it's a directory
    if (std::filesystem::is_directory(requested_path)) {
        // Look for index files
        for (const auto& index_file : config_.index_files) {
            auto index_path = requested_path / index_file;
            if (std::filesystem::exists(index_path) && std::filesystem::is_regular_file(index_path)) {
                return HttpResponse::conditional_file_response(index_path.string(), request);
            }
        }
        return create_error_response(HttpStatus::FORBIDDEN, "Directory listing disabled");
    }
    
    // Check if file exists
    if (!std::filesystem::exists(requested_path) || !std::filesystem::is_regular_file(requested_path)) {
        return create_error_response(HttpStatus::NOT_FOUND, "File not found");
    }
    
    return HttpResponse::conditional_file_response(requested_path.string(), request);
}

HttpResponse HttpServer::create_error_response(HttpStatus status, const std::string& message) {
    HttpResponse response(status);
    
    std::ostringstream html;
    html << "<!DOCTYPE html>\n"
         << "<html><head><title>" << static_cast<int>(status) << " " << HttpResponse::get_status_message(status) << "</title></head>\n"
         << "<body><h1>" << static_cast<int>(status) << " " << HttpResponse::get_status_message(status) << "</h1>\n"
         << "<p>" << message << "</p>\n"
         << "<hr><p>cpp-http-server/1.0</p></body></html>\n";
    
    response.set_html(html.str());
    return response;
}

void HttpServer::initialize_mime_types() {
    if (config_.mime_types.empty()) {
        config_.mime_types = {
            {"html", "text/html"},
            {"htm", "text/html"},
            {"css", "text/css"},
            {"js", "application/javascript"},
            {"json", "application/json"},
            {"xml", "application/xml"},
            {"txt", "text/plain"},
            {"png", "image/png"},
            {"jpg", "image/jpeg"},
            {"jpeg", "image/jpeg"},
            {"gif", "image/gif"},
            {"svg", "image/svg+xml"},
            {"ico", "image/x-icon"},
            {"pdf", "application/pdf"}
        };
    }
}

void HttpServer::log_request(const HttpRequest& request, const HttpResponse& response) {
    if (!config_.enable_logging) {
        return;
    }
    
    std::ostringstream log_entry;
    log_entry << "[" << get_current_timestamp() << "] "
              << HttpRequest::method_to_string(request.method()) << " "
              << request.path() << " "
              << static_cast<int>(response.status()) << " "
              << response.body().size() << " bytes";
    
    if (config_.log_file.empty()) {
        std::cout << log_entry.str() << std::endl;
    } else {
        std::ofstream log_file(config_.log_file, std::ios::app);
        if (log_file.is_open()) {
            log_file << log_entry.str() << std::endl;
        }
    }
}

std::string HttpServer::get_current_timestamp() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

bool HttpServer::path_matches(const std::string& pattern, const std::string& path) const {
    // Simple wildcard matching (could be enhanced with regex)
    if (pattern == path) {
        return true;
    }
    
    // Check for wildcard at end
    if (pattern.ends_with("*")) {
        std::string prefix = pattern.substr(0, pattern.length() - 1);
        return path.starts_with(prefix);
    }
    
    return false;
}

void HttpServer::accept_ssl_connections() {
    auto socket = std::make_shared<SslConnection::SslSocket>(io_context_, *ssl_context_);
    
    https_acceptor_->async_accept(socket->lowest_layer(),
        [this, socket](const boost::system::error_code& error) {
            handle_ssl_accept(error, socket);
        }
    );
}

void HttpServer::handle_ssl_accept(const boost::system::error_code& error, 
                                  std::shared_ptr<SslConnection::SslSocket> socket) {
    if (!error && running_.load()) {
        stats_.total_connections.fetch_add(1);
        stats_.active_connections.fetch_add(1);
        
        auto connection = std::make_shared<SslConnection>(
            std::move(*socket),
            [this](const HttpRequest& request) {
                stats_.total_requests.fetch_add(1);
                auto response = handle_request(request);
                log_request(request, response);
                return response;
            },
            [this]() {
                stats_.active_connections.fetch_sub(1);
            }
        );
        
        connection->start();
        
        // Accept next SSL connection
        accept_ssl_connections();
    } else if (error) {
        std::cerr << "HTTPS Accept error: " << error.message() << std::endl;
    }
}

void HttpServer::initialize_ssl_context() {
    if (!ssl_context_) {
        return;
    }
    
    try {
        ssl_context_->set_options(
            boost::asio::ssl::context::default_workarounds |
            boost::asio::ssl::context::no_sslv2 |
            boost::asio::ssl::context::no_sslv3 |
            boost::asio::ssl::context::single_dh_use
        );
        
        // Set cipher list
        if (!config_.ssl_cipher_list.empty()) {
            SSL_CTX_set_cipher_list(ssl_context_->native_handle(), config_.ssl_cipher_list.c_str());
        }
        
        // Load certificate chain
        if (!config_.ssl_certificate_file.empty()) {
            ssl_context_->use_certificate_chain_file(config_.ssl_certificate_file);
        }
        
        // Load private key
        if (!config_.ssl_private_key_file.empty()) {
            ssl_context_->use_private_key_file(config_.ssl_private_key_file, boost::asio::ssl::context::pem);
        }
        
        // Load CA file for client verification
        if (!config_.ssl_ca_file.empty()) {
            ssl_context_->load_verify_file(config_.ssl_ca_file);
            if (config_.ssl_verify_client) {
                ssl_context_->set_verify_mode(boost::asio::ssl::verify_peer | boost::asio::ssl::verify_fail_if_no_peer_cert);
            }
        }
        
        // Load DH parameters
        if (!config_.ssl_dh_file.empty()) {
            ssl_context_->use_tmp_dh_file(config_.ssl_dh_file);
        }
        
        // Set password callback if needed
        ssl_context_->set_password_callback([this](std::size_t, boost::asio::ssl::context::password_purpose) {
            return get_password();
        });
        
    } catch (const std::exception& e) {
        std::cerr << "SSL context initialization error: " << e.what() << std::endl;
        throw;
    }
}

std::string HttpServer::get_password() const {
    // In a real implementation, you might want to read this from a secure source
    // For now, return empty string (no password)
    return "";
}

} // namespace http_server