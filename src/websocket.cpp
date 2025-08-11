#include "websocket.hpp"
#include <iostream>
#include <sstream>
#include <random>
#include <algorithm>
#include <openssl/sha.h>
#include <openssl/evp.h>

namespace http_server {

namespace {
    const std::string WEBSOCKET_MAGIC_STRING = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    const std::string BASE64_CHARS = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    
    std::string base64_encode(const std::vector<uint8_t>& data) {
        std::string result;
        int val = 0, valb = -6;
        for (uint8_t c : data) {
            val = (val << 8) + c;
            valb += 8;
            while (valb >= 0) {
                result.push_back(BASE64_CHARS[(val >> valb) & 0x3F]);
                valb -= 6;
            }
        }
        if (valb > -6) {
            result.push_back(BASE64_CHARS[((val << 8) >> (valb + 8)) & 0x3F]);
        }
        while (result.size() % 4) {
            result.push_back('=');
        }
        return result;
    }
    
    std::vector<uint8_t> base64_decode(const std::string& data) {
        std::vector<uint8_t> result;
        std::vector<int> T(128, -1);
        for (int i = 0; i < 64; i++) {
            T[BASE64_CHARS[i]] = i;
        }
        
        int val = 0, valb = -8;
        for (uint8_t c : data) {
            if (T[c] == -1) break;
            val = (val << 6) + T[c];
            valb += 6;
            if (valb >= 0) {
                result.push_back(uint8_t((val >> valb) & 0xFF));
                valb -= 8;
            }
        }
        return result;
    }
    
    std::vector<uint8_t> sha1_hash(const std::string& data) {
        std::vector<uint8_t> hash(SHA_DIGEST_LENGTH);
        SHA1(reinterpret_cast<const unsigned char*>(data.c_str()), data.length(), hash.data());
        return hash;
    }
}

// WebSocketFrame implementation
std::vector<uint8_t> WebSocketFrame::serialize() const {
    std::vector<uint8_t> frame;
    
    // First byte: FIN + RSV + Opcode
    uint8_t first_byte = static_cast<uint8_t>(opcode);
    if (fin) first_byte |= 0x80;
    if (rsv1) first_byte |= 0x40;
    if (rsv2) first_byte |= 0x20;
    if (rsv3) first_byte |= 0x10;
    frame.push_back(first_byte);
    
    // Second byte: MASK + Payload length
    uint8_t second_byte = 0;
    if (masked) second_byte |= 0x80;
    
    if (payload_length < 126) {
        second_byte |= static_cast<uint8_t>(payload_length);
        frame.push_back(second_byte);
    } else if (payload_length < 65536) {
        second_byte |= 126;
        frame.push_back(second_byte);
        frame.push_back(static_cast<uint8_t>((payload_length >> 8) & 0xFF));
        frame.push_back(static_cast<uint8_t>(payload_length & 0xFF));
    } else {
        second_byte |= 127;
        frame.push_back(second_byte);
        for (int i = 7; i >= 0; --i) {
            frame.push_back(static_cast<uint8_t>((payload_length >> (i * 8)) & 0xFF));
        }
    }
    
    // Masking key (if present)
    if (masked) {
        frame.push_back(static_cast<uint8_t>((masking_key >> 24) & 0xFF));
        frame.push_back(static_cast<uint8_t>((masking_key >> 16) & 0xFF));
        frame.push_back(static_cast<uint8_t>((masking_key >> 8) & 0xFF));
        frame.push_back(static_cast<uint8_t>(masking_key & 0xFF));
    }
    
    // Payload
    if (masked) {
        auto masked_payload = payload;
        for (size_t i = 0; i < masked_payload.size(); ++i) {
            masked_payload[i] ^= static_cast<uint8_t>((masking_key >> ((3 - (i % 4)) * 8)) & 0xFF);
        }
        frame.insert(frame.end(), masked_payload.begin(), masked_payload.end());
    } else {
        frame.insert(frame.end(), payload.begin(), payload.end());
    }
    
    return frame;
}

WebSocketFrame WebSocketFrame::parse(const std::vector<uint8_t>& data, size_t& bytes_consumed) {
    bytes_consumed = 0;
    if (data.size() < 2) {
        throw std::runtime_error("Insufficient data for WebSocket frame header");
    }
    
    WebSocketFrame frame;
    size_t offset = 0;
    
    // Parse first byte
    uint8_t first_byte = data[offset++];
    frame.fin = (first_byte & 0x80) != 0;
    frame.rsv1 = (first_byte & 0x40) != 0;
    frame.rsv2 = (first_byte & 0x20) != 0;
    frame.rsv3 = (first_byte & 0x10) != 0;
    frame.opcode = static_cast<WebSocketOpcode>(first_byte & 0x0F);
    
    // Parse second byte
    uint8_t second_byte = data[offset++];
    frame.masked = (second_byte & 0x80) != 0;
    uint64_t payload_len = second_byte & 0x7F;
    
    // Parse extended payload length
    if (payload_len == 126) {
        if (data.size() < offset + 2) {
            throw std::runtime_error("Insufficient data for 16-bit payload length");
        }
        payload_len = (static_cast<uint64_t>(data[offset]) << 8) | data[offset + 1];
        offset += 2;
    } else if (payload_len == 127) {
        if (data.size() < offset + 8) {
            throw std::runtime_error("Insufficient data for 64-bit payload length");
        }
        payload_len = 0;
        for (int i = 0; i < 8; ++i) {
            payload_len = (payload_len << 8) | data[offset + i];
        }
        offset += 8;
    }
    
    frame.payload_length = payload_len;
    
    // Parse masking key
    if (frame.masked) {
        if (data.size() < offset + 4) {
            throw std::runtime_error("Insufficient data for masking key");
        }
        frame.masking_key = (static_cast<uint32_t>(data[offset]) << 24) |
                           (static_cast<uint32_t>(data[offset + 1]) << 16) |
                           (static_cast<uint32_t>(data[offset + 2]) << 8) |
                           static_cast<uint32_t>(data[offset + 3]);
        offset += 4;
    }
    
    // Parse payload
    if (data.size() < offset + payload_len) {
        throw std::runtime_error("Insufficient data for payload");
    }
    
    frame.payload.assign(data.begin() + offset, data.begin() + offset + payload_len);
    
    // Unmask payload if necessary
    if (frame.masked) {
        for (size_t i = 0; i < frame.payload.size(); ++i) {
            frame.payload[i] ^= static_cast<uint8_t>((frame.masking_key >> ((3 - (i % 4)) * 8)) & 0xFF);
        }
    }
    
    bytes_consumed = offset + payload_len;
    return frame;
}

// WebSocketConnection implementation
WebSocketConnection::WebSocketConnection(boost::asio::ip::tcp::socket socket)
    : socket_(std::move(socket))
    , state_(WebSocketState::CONNECTING)
    , creation_time_(std::chrono::steady_clock::now())
    , ping_timer_(socket_.get_executor())
    , timeout_timer_(socket_.get_executor()) {
}

WebSocketConnection::~WebSocketConnection() {
    if (state_ != WebSocketState::CLOSED) {
        // Don't use async close() in destructor to avoid shared_from_this issues
        state_ = WebSocketState::CLOSED;
        boost::system::error_code ec;
        socket_.close(ec);
    }
}

bool WebSocketConnection::handshake(const HttpRequest& request) {
    if (!WebSocketUtils::is_websocket_request(request)) {
        return false;
    }
    
    if (!WebSocketUtils::validate_websocket_version(request)) {
        return false;
    }
    
    auto key_header = request.get_header("Sec-WebSocket-Key");
    if (!key_header || !WebSocketUtils::validate_websocket_key(*key_header)) {
        return false;
    }
    
    // Generate response
    HttpResponse response = WebSocketUtils::create_handshake_response(request);
    std::string response_str = response.to_http_string();
    
    // Send handshake response
    boost::system::error_code ec;
    boost::asio::write(socket_, boost::asio::buffer(response_str), ec);
    
    if (ec) {
        handle_error("Failed to send WebSocket handshake response: " + ec.message());
        return false;
    }
    
    state_ = WebSocketState::OPEN;
    return true;
}

void WebSocketConnection::start() {
    if (state_ != WebSocketState::OPEN) {
        return;
    }
    
    setup_ping_timer();
    setup_timeout();
    read_frame();
}

void WebSocketConnection::close(uint16_t code, const std::string& reason) {
    if (state_ == WebSocketState::CLOSED || state_ == WebSocketState::CLOSING) {
        return;
    }
    
    state_ = WebSocketState::CLOSING;
    
    // Send close frame
    WebSocketFrame frame;
    frame.fin = true;
    frame.opcode = WebSocketOpcode::CLOSE;
    
    if (code != 0) {
        frame.payload.push_back(static_cast<uint8_t>((code >> 8) & 0xFF));
        frame.payload.push_back(static_cast<uint8_t>(code & 0xFF));
        frame.payload.insert(frame.payload.end(), reason.begin(), reason.end());
    }
    
    frame.payload_length = frame.payload.size();
    send_frame(frame);
    
    // Close socket after a brief delay
    auto self = shared_from_this();
    timeout_timer_.expires_after(std::chrono::milliseconds(100));
    timeout_timer_.async_wait([self](const boost::system::error_code&) {
        self->socket_.close();
        self->state_ = WebSocketState::CLOSED;
    });
}

void WebSocketConnection::send_text(const std::string& message) {
    if (state_ != WebSocketState::OPEN) {
        return;
    }
    
    WebSocketFrame frame;
    frame.fin = true;
    frame.opcode = WebSocketOpcode::TEXT;
    frame.payload.assign(message.begin(), message.end());
    frame.payload_length = frame.payload.size();
    
    send_frame(frame);
    ++messages_sent_;
}

void WebSocketConnection::send_binary(const std::vector<uint8_t>& data) {
    if (state_ != WebSocketState::OPEN) {
        return;
    }
    
    WebSocketFrame frame;
    frame.fin = true;
    frame.opcode = WebSocketOpcode::BINARY;
    frame.payload = data;
    frame.payload_length = frame.payload.size();
    
    send_frame(frame);
    ++messages_sent_;
}

void WebSocketConnection::send_ping(const std::vector<uint8_t>& data) {
    if (state_ != WebSocketState::OPEN) {
        return;
    }
    
    WebSocketFrame frame;
    frame.fin = true;
    frame.opcode = WebSocketOpcode::PING;
    frame.payload = data;
    frame.payload_length = frame.payload.size();
    
    send_frame(frame);
}

void WebSocketConnection::send_pong(const std::vector<uint8_t>& data) {
    if (state_ != WebSocketState::OPEN) {
        return;
    }
    
    WebSocketFrame frame;
    frame.fin = true;
    frame.opcode = WebSocketOpcode::PONG;
    frame.payload = data;
    frame.payload_length = frame.payload.size();
    
    send_frame(frame);
}

std::string WebSocketConnection::client_address() const {
    try {
        return socket_.remote_endpoint().address().to_string();
    } catch (...) {
        return "unknown";
    }
}

std::string WebSocketConnection::client_port() const {
    try {
        return std::to_string(socket_.remote_endpoint().port());
    } catch (...) {
        return "0";
    }
}

void WebSocketConnection::read_frame() {
    auto self = shared_from_this();
    socket_.async_read_some(
        boost::asio::buffer(buffer_),
        [self](const boost::system::error_code& error, size_t bytes_transferred) {
            self->handle_read(error, bytes_transferred);
        }
    );
}

void WebSocketConnection::handle_read(const boost::system::error_code& error, size_t bytes_transferred) {
    if (error) {
        handle_error("Read error: " + error.message());
        return;
    }
    
    bytes_received_ += bytes_transferred;
    frame_buffer_.insert(frame_buffer_.end(), buffer_.begin(), buffer_.begin() + bytes_transferred);
    
    process_frames();
    
    if (state_ == WebSocketState::OPEN) {
        read_frame();
    }
}

void WebSocketConnection::process_frames() {
    while (!frame_buffer_.empty()) {
        try {
            size_t bytes_consumed = 0;
            WebSocketFrame frame = WebSocketFrame::parse(frame_buffer_, bytes_consumed);
            
            frame_buffer_.erase(frame_buffer_.begin(), frame_buffer_.begin() + bytes_consumed);
            handle_frame(frame);
            
        } catch (const std::runtime_error&) {
            // Incomplete frame, wait for more data
            break;
        }
    }
}

void WebSocketConnection::handle_frame(const WebSocketFrame& frame) {
    switch (frame.opcode) {
        case WebSocketOpcode::TEXT:
            if (text_handler_) {
                std::string message(frame.payload.begin(), frame.payload.end());
                text_handler_(message);
            }
            ++messages_received_;
            break;
            
        case WebSocketOpcode::BINARY:
            if (binary_handler_) {
                binary_handler_(frame.payload);
            }
            ++messages_received_;
            break;
            
        case WebSocketOpcode::CLOSE:
            handle_close_frame(frame.payload);
            break;
            
        case WebSocketOpcode::PING:
            handle_ping(frame.payload);
            break;
            
        case WebSocketOpcode::PONG:
            handle_pong(frame.payload);
            break;
            
        default:
            // Handle other opcodes as needed
            break;
    }
}

void WebSocketConnection::send_frame(const WebSocketFrame& frame) {
    auto data = std::make_shared<std::vector<uint8_t>>(frame.serialize());
    
    auto self = shared_from_this();
    boost::asio::async_write(
        socket_,
        boost::asio::buffer(*data),
        [self, data](const boost::system::error_code& error, size_t bytes_transferred) {
            self->handle_write(error, bytes_transferred, data);
        }
    );
}

void WebSocketConnection::handle_write(const boost::system::error_code& error, size_t bytes_transferred,
                                     std::shared_ptr<std::vector<uint8_t>>) {
    if (error) {
        handle_error("Write error: " + error.message());
        return;
    }
    
    bytes_sent_ += bytes_transferred;
}

void WebSocketConnection::handle_ping(const std::vector<uint8_t>& data) {
    send_pong(data);
}

void WebSocketConnection::handle_pong(const std::vector<uint8_t>&) {
    // Reset timeout on pong
    setup_timeout();
}

void WebSocketConnection::handle_close_frame(const std::vector<uint8_t>& data) {
    uint16_t code = 1000;
    std::string reason;
    
    if (data.size() >= 2) {
        code = (static_cast<uint16_t>(data[0]) << 8) | data[1];
        if (data.size() > 2) {
            reason = std::string(data.begin() + 2, data.end());
        }
    }
    
    if (close_handler_) {
        close_handler_(code, reason);
    }
    
    state_ = WebSocketState::CLOSED;
    socket_.close();
}

void WebSocketConnection::setup_ping_timer() {
    auto self = shared_from_this();
    ping_timer_.expires_after(PING_INTERVAL);
    ping_timer_.async_wait([self](const boost::system::error_code& error) {
        self->handle_ping_timer(error);
    });
}

void WebSocketConnection::handle_ping_timer(const boost::system::error_code& error) {
    if (error || state_ != WebSocketState::OPEN) {
        return;
    }
    
    send_ping();
    setup_ping_timer();
}

void WebSocketConnection::setup_timeout() {
    auto self = shared_from_this();
    timeout_timer_.expires_after(TIMEOUT);
    timeout_timer_.async_wait([self](const boost::system::error_code& error) {
        self->handle_timeout(error);
    });
}

void WebSocketConnection::handle_timeout(const boost::system::error_code& error) {
    if (error || state_ != WebSocketState::OPEN) {
        return;
    }
    
    handle_error("Connection timeout");
}

void WebSocketConnection::handle_error(const std::string& error) {
    if (error_handler_) {
        error_handler_(error);
    }
    
    state_ = WebSocketState::CLOSED;
    socket_.close();
}

std::string WebSocketConnection::generate_accept_key(const std::string& key) {
    return WebSocketUtils::compute_accept_key(key);
}

// WebSocketUtils implementation
std::string WebSocketUtils::generate_websocket_key() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);
    
    std::vector<uint8_t> key_bytes(16);
    for (auto& byte : key_bytes) {
        byte = static_cast<uint8_t>(dis(gen));
    }
    
    return base64_encode(key_bytes);
}

std::string WebSocketUtils::compute_accept_key(const std::string& key) {
    std::string combined = key + WEBSOCKET_MAGIC_STRING;
    auto hash = sha1_hash(combined);
    return base64_encode(hash);
}

bool WebSocketUtils::is_websocket_request(const HttpRequest& request) {
    auto upgrade = request.get_header("Upgrade");
    auto connection = request.get_header("Connection");
    
    return upgrade && *upgrade == "websocket" &&
           connection && connection->find("Upgrade") != std::string::npos;
}

bool WebSocketUtils::validate_websocket_version(const HttpRequest& request) {
    auto version = request.get_header("Sec-WebSocket-Version");
    return version && *version == "13";
}

bool WebSocketUtils::validate_websocket_key(const std::string& key) {
    if (key.empty()) return false;
    
    try {
        auto decoded = base64_decode(key);
        return decoded.size() == 16;
    } catch (...) {
        return false;
    }
}

HttpResponse WebSocketUtils::create_handshake_response(const HttpRequest& request) {
    auto key = request.get_header("Sec-WebSocket-Key");
    if (!key) {
        return create_handshake_rejection("Missing Sec-WebSocket-Key");
    }
    
    std::string accept_key = compute_accept_key(*key);
    
    HttpResponse response(HttpStatus::SWITCHING_PROTOCOLS);
    response.set_header("Upgrade", "websocket");
    response.set_header("Connection", "Upgrade");
    response.set_header("Sec-WebSocket-Accept", accept_key);
    
    return response;
}

HttpResponse WebSocketUtils::create_handshake_rejection(const std::string& reason) {
    HttpResponse response(HttpStatus::BAD_REQUEST);
    response.set_text("WebSocket handshake failed");
    if (!reason.empty()) {
        response.set_header("X-WebSocket-Reject-Reason", reason);
    }
    return response;
}

} // namespace http_server
