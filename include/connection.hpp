#pragma once

#include <memory>
#include <string>
#include <functional>
#include <chrono>
#include <boost/asio.hpp>
#include "request.hpp"
#include "response.hpp"

namespace http_server {

class HttpServer; // Forward declaration

class Connection : public std::enable_shared_from_this<Connection> {
public:
    using RequestHandler = std::function<HttpResponse(const HttpRequest&)>;
    
    explicit Connection(boost::asio::ip::tcp::socket socket, RequestHandler handler, 
                       std::function<void()> cleanup_callback = nullptr);
    ~Connection();
    
    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;
    
    void start();
    
    std::string client_address() const;
    std::string client_port() const;
    
    bool is_open() const;
    void close();
    
    std::chrono::steady_clock::time_point creation_time() const noexcept { return creation_time_; }
    size_t bytes_received() const noexcept { return bytes_received_; }
    size_t bytes_sent() const noexcept { return bytes_sent_; }

private:
    boost::asio::ip::tcp::socket socket_;
    RequestHandler request_handler_;
    std::function<void()> cleanup_callback_;
    std::array<char, 8192> buffer_;
    std::string request_data_;
    std::chrono::steady_clock::time_point creation_time_;
    size_t bytes_received_{0};
    size_t bytes_sent_{0};
    
    static constexpr size_t MAX_REQUEST_SIZE = 1024 * 1024; // 1MB
    static constexpr std::chrono::seconds TIMEOUT{30}; // 30 seconds
    
    void read_request();
    void handle_read(const boost::system::error_code& error, size_t bytes_transferred);
    void process_request();
    void send_response(const HttpResponse& response);
    void write_body_chunk(const HttpResponse& response, std::shared_ptr<std::istream> body_stream);
    void handle_write(const boost::system::error_code& error, size_t bytes_transferred, 
                     std::shared_ptr<std::string> response_data);
    
    bool is_request_complete() const;
    void handle_error(const boost::system::error_code& error);
    void setup_timeout();
    
    boost::asio::steady_timer timeout_timer_;
    void handle_timeout(const boost::system::error_code& error);
};

using ConnectionPtr = std::shared_ptr<Connection>;

} // namespace http_server
