#pragma once

#include <memory>
#include <string>
#include <functional>
#include <atomic>
#include <unordered_map>
#include <filesystem>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <nlohmann/json.hpp>
#include "connection.hpp"
#include "ssl_connection.hpp"
#include "request.hpp"
#include "response.hpp"
#include "thread_pool.hpp"

namespace http_server {

struct ServerConfig {
    std::string host{"0.0.0.0"};
    uint16_t port{8080};
    size_t thread_pool_size{std::thread::hardware_concurrency()};
    std::string document_root{"./public"};
    size_t max_connections{1000};
    std::chrono::seconds keep_alive_timeout{30};
    size_t max_request_size{1024 * 1024}; // 1MB
    bool enable_logging{true};
    std::string log_file{"server.log"};
    
    // HTTPS Configuration
    bool enable_https{false};
    uint16_t https_port{8443};
    std::string ssl_certificate_file;
    std::string ssl_private_key_file;
    std::string ssl_ca_file; // Optional: for client certificate verification
    std::string ssl_dh_file; // Optional: for DHE ciphers
    bool ssl_verify_client{false};
    std::string ssl_cipher_list{"HIGH:!aNULL:!MD5"};
    
    bool serve_static_files{true};
    std::vector<std::string> index_files{"index.html", "index.htm"};
    
    bool enable_compression{true};
    size_t compression_min_size{1024};
    int compression_level{6};
    std::vector<std::string> compressible_types{
        "text/plain", "text/html", "text/css", "application/javascript", 
        "application/json", "application/xml", "text/xml"
    };
    
    std::unordered_map<std::string, std::string> mime_types;
    
    static ServerConfig from_json(const std::string& config_file);
    static ServerConfig from_json_string(const std::string& json_str);
    nlohmann::json to_json() const;
};

class HttpServer {
public:
    using RequestHandler = std::function<HttpResponse(const HttpRequest&)>;
    using MiddlewareHandler = std::function<bool(const HttpRequest&, HttpResponse&)>;
    
    explicit HttpServer(const ServerConfig& config = ServerConfig{});
    ~HttpServer();
    
    HttpServer(const HttpServer&) = delete;
    HttpServer& operator=(const HttpServer&) = delete;
    
    void start();
    void stop();
    bool is_running() const noexcept { return running_.load(); }
    
    void add_route(const std::string& path, HttpMethod method, RequestHandler handler);
    void add_get_route(const std::string& path, RequestHandler handler);
    void add_post_route(const std::string& path, RequestHandler handler);
    void add_put_route(const std::string& path, RequestHandler handler);
    void add_delete_route(const std::string& path, RequestHandler handler);
    void add_patch_route(const std::string& path, RequestHandler handler);
    
    void add_middleware(MiddlewareHandler middleware);
    
    void enable_static_files(const std::string& document_root);
    void disable_static_files();
    
    const ServerConfig& config() const noexcept { return config_; }
    void update_config(const ServerConfig& new_config);
    
    struct Statistics {
        std::atomic<size_t> total_requests{0};
        std::atomic<size_t> active_connections{0};
        std::atomic<size_t> total_connections{0};
        std::atomic<size_t> bytes_sent{0};
        std::atomic<size_t> bytes_received{0};
        std::chrono::steady_clock::time_point start_time;
    };
    
    const Statistics& stats() const noexcept { return stats_; }
    std::string stats_json() const;

private:
    ServerConfig config_;
    boost::asio::io_context io_context_;
    boost::asio::ip::tcp::acceptor acceptor_;
    std::unique_ptr<boost::asio::ip::tcp::acceptor> https_acceptor_;
    std::unique_ptr<boost::asio::ssl::context> ssl_context_;
    std::unique_ptr<ThreadPool> thread_pool_;
    std::atomic<bool> running_{false};
    mutable Statistics stats_;
    
    struct RouteKey {
        std::string path;
        HttpMethod method;
        
        bool operator==(const RouteKey& other) const {
            return path == other.path && method == other.method;
        }
    };
    
    struct RouteKeyHash {
        size_t operator()(const RouteKey& key) const {
            return std::hash<std::string>{}(key.path) ^ 
                   std::hash<int>{}(static_cast<int>(key.method));
        }
    };
    
    std::unordered_map<RouteKey, RequestHandler, RouteKeyHash> routes_;
    std::vector<MiddlewareHandler> middleware_;
    
    void accept_connections();
    void handle_accept(const boost::system::error_code& error, 
                      boost::asio::ip::tcp::socket socket);
    
    void accept_ssl_connections();
    void handle_ssl_accept(const boost::system::error_code& error, 
                          std::shared_ptr<SslConnection::SslSocket> socket);
    
    void initialize_ssl_context();
    std::string get_password() const;
    
    HttpResponse handle_request(const HttpRequest& request);
    HttpResponse handle_static_file(const HttpRequest& request);
    HttpResponse create_error_response(HttpStatus status, const std::string& message = "");
    
    void initialize_mime_types();
    void log_request(const HttpRequest& request, const HttpResponse& response);
    std::string get_current_timestamp() const;
    bool path_matches(const std::string& pattern, const std::string& path) const;
};

} // namespace http_server
