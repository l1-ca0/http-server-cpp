#pragma once

#include <memory>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include "request.hpp"
#include "response.hpp"

namespace http_server {

/**
 * @brief SSL/TLS connection handler that wraps the base Connection functionality
 */
class SslConnection : public std::enable_shared_from_this<SslConnection> {
public:
    using SslSocket = boost::asio::ssl::stream<boost::asio::ip::tcp::socket>;
    using RequestHandler = std::function<HttpResponse(const HttpRequest&)>;
    
    SslConnection(SslSocket socket, RequestHandler handler, std::function<void()> cleanup_callback);
    ~SslConnection();
    
    void start();
    void close();
    
    std::string client_address() const;
    std::string client_port() const;
    bool is_open() const;
    
    size_t bytes_sent() const noexcept { return bytes_sent_; }
    size_t bytes_received() const noexcept { return bytes_received_; }
    std::chrono::steady_clock::time_point creation_time() const noexcept { return creation_time_; }

private:
    static constexpr size_t BUFFER_SIZE = 8192;
    static constexpr auto TIMEOUT = std::chrono::seconds(30);
    static constexpr size_t MAX_REQUEST_SIZE = 1024 * 1024; // 1MB
    
    SslSocket socket_;
    RequestHandler request_handler_;
    std::function<void()> cleanup_callback_;
    
    std::array<char, BUFFER_SIZE> buffer_;
    std::string request_data_;
    size_t bytes_sent_{0};
    size_t bytes_received_{0};
    std::chrono::steady_clock::time_point creation_time_;
    boost::asio::steady_timer timeout_timer_;
    
    void handle_handshake(const boost::system::error_code& error);
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
    void handle_timeout(const boost::system::error_code& error);
};

} // namespace http_server
