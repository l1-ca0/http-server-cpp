#pragma once

#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <cstdint>
#include <boost/asio.hpp>
#include "request.hpp"
#include "response.hpp"

namespace http_server {

/**
 * @brief WebSocket opcodes as defined in RFC 6455
 */
enum class WebSocketOpcode : uint8_t {
    CONTINUATION = 0x0,
    TEXT = 0x1,
    BINARY = 0x2,
    CLOSE = 0x8,
    PING = 0x9,
    PONG = 0xA
};

/**
 * @brief WebSocket frame structure
 */
struct WebSocketFrame {
    bool fin = true;
    bool rsv1 = false;
    bool rsv2 = false;
    bool rsv3 = false;
    WebSocketOpcode opcode = WebSocketOpcode::TEXT;
    bool masked = false;
    uint64_t payload_length = 0;
    uint32_t masking_key = 0;
    std::vector<uint8_t> payload;
    
    std::vector<uint8_t> serialize() const;
    static WebSocketFrame parse(const std::vector<uint8_t>& data, size_t& bytes_consumed);
};

/**
 * @brief WebSocket connection state
 */
enum class WebSocketState {
    CONNECTING,
    OPEN,
    CLOSING,
    CLOSED
};

/**
 * @brief WebSocket connection handler for real-time communication
 */
class WebSocketConnection : public std::enable_shared_from_this<WebSocketConnection> {
public:
    using MessageHandler = std::function<void(const std::string&)>;
    using BinaryHandler = std::function<void(const std::vector<uint8_t>&)>;
    using CloseHandler = std::function<void(uint16_t code, const std::string& reason)>;
    using ErrorHandler = std::function<void(const std::string& error)>;
    
    explicit WebSocketConnection(boost::asio::ip::tcp::socket socket);
    ~WebSocketConnection();
    
    // Connection management
    bool handshake(const HttpRequest& request);
    void start();
    void close(uint16_t code = 1000, const std::string& reason = "");
    
    // Message sending
    void send_text(const std::string& message);
    void send_binary(const std::vector<uint8_t>& data);
    void send_ping(const std::vector<uint8_t>& data = {});
    void send_pong(const std::vector<uint8_t>& data = {});
    
    // Event handlers
    void on_message(MessageHandler handler) { text_handler_ = std::move(handler); }
    void on_binary(BinaryHandler handler) { binary_handler_ = std::move(handler); }
    void on_close(CloseHandler handler) { close_handler_ = std::move(handler); }
    void on_error(ErrorHandler handler) { error_handler_ = std::move(handler); }
    
    // State queries
    WebSocketState state() const noexcept { return state_; }
    bool is_open() const noexcept { return state_ == WebSocketState::OPEN; }
    std::string client_address() const;
    std::string client_port() const;
    
    // Statistics
    size_t bytes_sent() const noexcept { return bytes_sent_; }
    size_t bytes_received() const noexcept { return bytes_received_; }
    size_t messages_sent() const noexcept { return messages_sent_; }
    size_t messages_received() const noexcept { return messages_received_; }
    std::chrono::steady_clock::time_point creation_time() const noexcept { return creation_time_; }

private:
    static constexpr size_t BUFFER_SIZE = 8192;
    static constexpr auto PING_INTERVAL = std::chrono::seconds(30);
    static constexpr auto TIMEOUT = std::chrono::seconds(60);
    
    boost::asio::ip::tcp::socket socket_;
    WebSocketState state_;
    
    std::array<char, BUFFER_SIZE> buffer_;
    std::vector<uint8_t> frame_buffer_;
    std::vector<uint8_t> message_buffer_;
    
    // Event handlers
    MessageHandler text_handler_;
    BinaryHandler binary_handler_;
    CloseHandler close_handler_;
    ErrorHandler error_handler_;
    
    // Statistics
    size_t bytes_sent_{0};
    size_t bytes_received_{0};
    size_t messages_sent_{0};
    size_t messages_received_{0};
    std::chrono::steady_clock::time_point creation_time_;
    
    // Timers
    boost::asio::steady_timer ping_timer_;
    boost::asio::steady_timer timeout_timer_;
    
    // Frame processing
    void read_frame();
    void handle_read(const boost::system::error_code& error, size_t bytes_transferred);
    void process_frames();
    void handle_frame(const WebSocketFrame& frame);
    
    void send_frame(const WebSocketFrame& frame);
    void handle_write(const boost::system::error_code& error, size_t bytes_transferred,
                     std::shared_ptr<std::vector<uint8_t>> data);
    
    // Protocol handling
    void handle_ping(const std::vector<uint8_t>& data);
    void handle_pong(const std::vector<uint8_t>& data);
    void handle_close_frame(const std::vector<uint8_t>& data);
    
    // Timers
    void setup_ping_timer();
    void handle_ping_timer(const boost::system::error_code& error);
    void setup_timeout();
    void handle_timeout(const boost::system::error_code& error);
    
    // Utilities
    void handle_error(const std::string& error);
    std::string generate_accept_key(const std::string& key);
    void apply_mask(std::vector<uint8_t>& data, uint32_t mask);
};

/**
 * @brief WebSocket utility functions
 */
class WebSocketUtils {
public:
    // WebSocket key generation and validation
    static std::string generate_websocket_key();
    static std::string compute_accept_key(const std::string& key);
    static bool is_websocket_request(const HttpRequest& request);
    
    // Protocol validation
    static bool validate_websocket_version(const HttpRequest& request);
    static bool validate_websocket_key(const std::string& key);
    
    // Response generation
    static HttpResponse create_handshake_response(const HttpRequest& request);
    static HttpResponse create_handshake_rejection(const std::string& reason = "");
};

} // namespace http_server
