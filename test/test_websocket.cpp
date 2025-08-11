#include <gtest/gtest.h>
#include "websocket.hpp"
#include "server.hpp"
#include <thread>
#include <chrono>
#include <future>

using namespace http_server;

class WebSocketTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_.port = 8090;  // Use different port for WebSocket tests
        config_.thread_pool_size = 2;
        server_ = std::make_unique<HttpServer>(config_);
    }
    
    void TearDown() override {
        if (server_->is_running()) {
            server_->stop();
        }
    }
    
    ServerConfig config_;
    std::unique_ptr<HttpServer> server_;
};

// Test WebSocket frame serialization and parsing
TEST_F(WebSocketTest, FrameSerializationAndParsing) {
    // Test text frame
    WebSocketFrame frame;
    frame.fin = true;
    frame.opcode = WebSocketOpcode::TEXT;
    frame.payload = {'H', 'e', 'l', 'l', 'o'};
    frame.payload_length = frame.payload.size();
    
    auto serialized = frame.serialize();
    ASSERT_FALSE(serialized.empty());
    
    size_t bytes_consumed = 0;
    auto parsed = WebSocketFrame::parse(serialized, bytes_consumed);
    
    EXPECT_EQ(parsed.fin, frame.fin);
    EXPECT_EQ(parsed.opcode, frame.opcode);
    EXPECT_EQ(parsed.payload, frame.payload);
    EXPECT_EQ(bytes_consumed, serialized.size());
}

// Test binary frame
TEST_F(WebSocketTest, BinaryFrameHandling) {
    WebSocketFrame frame;
    frame.fin = true;
    frame.opcode = WebSocketOpcode::BINARY;
    frame.payload = {0x01, 0x02, 0x03, 0x04, 0xFF};
    frame.payload_length = frame.payload.size();
    
    auto serialized = frame.serialize();
    size_t bytes_consumed = 0;
    auto parsed = WebSocketFrame::parse(serialized, bytes_consumed);
    
    EXPECT_EQ(parsed.opcode, WebSocketOpcode::BINARY);
    EXPECT_EQ(parsed.payload, frame.payload);
}

// Test control frames
TEST_F(WebSocketTest, ControlFrames) {
    // Test PING frame
    WebSocketFrame ping_frame;
    ping_frame.fin = true;
    ping_frame.opcode = WebSocketOpcode::PING;
    ping_frame.payload = {'p', 'i', 'n', 'g'};
    ping_frame.payload_length = ping_frame.payload.size();
    
    auto serialized = ping_frame.serialize();
    size_t bytes_consumed = 0;
    auto parsed = WebSocketFrame::parse(serialized, bytes_consumed);
    
    EXPECT_EQ(parsed.opcode, WebSocketOpcode::PING);
    EXPECT_EQ(parsed.payload, ping_frame.payload);
    
    // Test CLOSE frame
    WebSocketFrame close_frame;
    close_frame.fin = true;
    close_frame.opcode = WebSocketOpcode::CLOSE;
    close_frame.payload = {0x03, 0xE8}; // 1000 in big-endian
    close_frame.payload_length = close_frame.payload.size();
    
    serialized = close_frame.serialize();
    bytes_consumed = 0;
    parsed = WebSocketFrame::parse(serialized, bytes_consumed);
    
    EXPECT_EQ(parsed.opcode, WebSocketOpcode::CLOSE);
    EXPECT_EQ(parsed.payload, close_frame.payload);
}

// Test masked frames (client-to-server)
TEST_F(WebSocketTest, MaskedFrames) {
    WebSocketFrame frame;
    frame.fin = true;
    frame.opcode = WebSocketOpcode::TEXT;
    frame.masked = true;
    frame.masking_key = 0x12345678;
    frame.payload = {'H', 'e', 'l', 'l', 'o'};
    frame.payload_length = frame.payload.size();
    
    auto serialized = frame.serialize();
    size_t bytes_consumed = 0;
    auto parsed = WebSocketFrame::parse(serialized, bytes_consumed);
    
    EXPECT_EQ(parsed.masked, true);
    EXPECT_EQ(parsed.masking_key, frame.masking_key);
    EXPECT_EQ(parsed.payload, frame.payload); // Should be automatically unmasked
}

// Test WebSocket key generation and validation
TEST_F(WebSocketTest, KeyGeneration) {
    std::string key = WebSocketUtils::generate_websocket_key();
    EXPECT_FALSE(key.empty());
    EXPECT_TRUE(WebSocketUtils::validate_websocket_key(key));
    
    std::string accept_key = WebSocketUtils::compute_accept_key(key);
    EXPECT_FALSE(accept_key.empty());
    EXPECT_NE(key, accept_key);
}

// Test WebSocket request validation
TEST_F(WebSocketTest, RequestValidation) {
    // Create a valid WebSocket request by parsing raw HTTP
    std::string raw_request = 
        "GET /websocket HTTP/1.1\r\n"
        "Host: localhost:8080\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "\r\n";
    
    auto request_opt = HttpRequest::parse(raw_request);
    ASSERT_TRUE(request_opt.has_value());
    
    const auto& request = request_opt.value();
    
    EXPECT_TRUE(WebSocketUtils::is_websocket_request(request));
    EXPECT_TRUE(WebSocketUtils::validate_websocket_version(request));
    
    auto key = request.get_header("Sec-WebSocket-Key");
    ASSERT_TRUE(key);
    EXPECT_TRUE(WebSocketUtils::validate_websocket_key(*key));
}

// Test invalid WebSocket requests
TEST_F(WebSocketTest, InvalidRequestHandling) {
    // Missing upgrade header
    std::string raw_request = 
        "GET /websocket HTTP/1.1\r\n"
        "Host: localhost:8080\r\n"
        "\r\n";
    
    auto request_opt = HttpRequest::parse(raw_request);
    ASSERT_TRUE(request_opt.has_value());
    
    const auto& request = request_opt.value();
    EXPECT_FALSE(WebSocketUtils::is_websocket_request(request));
    
    // Wrong upgrade header
    std::string raw_request2 = 
        "GET /websocket HTTP/1.1\r\n"
        "Host: localhost:8080\r\n"
        "Upgrade: h2c\r\n"
        "Connection: Upgrade\r\n"
        "\r\n";
    
    auto request_opt2 = HttpRequest::parse(raw_request2);
    ASSERT_TRUE(request_opt2.has_value());
    
    const auto& request2 = request_opt2.value();
    EXPECT_FALSE(WebSocketUtils::is_websocket_request(request2));
    
    // Wrong version
    std::string raw_request3 = 
        "GET /websocket HTTP/1.1\r\n"
        "Host: localhost:8080\r\n"
        "Upgrade: websocket\r\n"
        "Sec-WebSocket-Version: 8\r\n"
        "\r\n";
    
    auto request_opt3 = HttpRequest::parse(raw_request3);
    ASSERT_TRUE(request_opt3.has_value());
    
    const auto& request3 = request_opt3.value();
    EXPECT_FALSE(WebSocketUtils::validate_websocket_version(request3));
}

// Test handshake response generation
TEST_F(WebSocketTest, HandshakeResponseGeneration) {
    std::string raw_request = 
        "GET /websocket HTTP/1.1\r\n"
        "Host: localhost:8080\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "\r\n";
    
    auto request_opt = HttpRequest::parse(raw_request);
    ASSERT_TRUE(request_opt.has_value());
    
    const auto& request = request_opt.value();
    auto response = WebSocketUtils::create_handshake_response(request);
    
    EXPECT_EQ(response.status(), HttpStatus::SWITCHING_PROTOCOLS);
    
    std::string upgrade_header = response.get_header("Upgrade");
    EXPECT_EQ(upgrade_header, "websocket");
    
    std::string connection_header = response.get_header("Connection");
    EXPECT_EQ(connection_header, "Upgrade");
    
    std::string accept_header = response.get_header("Sec-WebSocket-Accept");
    EXPECT_FALSE(accept_header.empty());
}

// Test WebSocket route registration
TEST_F(WebSocketTest, RouteRegistration) {
    bool handler_called = false;
    std::shared_ptr<WebSocketConnection> received_connection;
    
    server_->add_websocket_route("/ws", [&](std::shared_ptr<WebSocketConnection> conn) {
        handler_called = true;
        received_connection = conn;
    });
    
    // Note: This test only verifies route registration
    // Full integration testing would require actually connecting
    EXPECT_TRUE(true); // Route registration doesn't throw
}

// Test server statistics with WebSocket connections
TEST_F(WebSocketTest, ServerStatistics) {
    const auto& stats = server_->stats();
    
    // Initial state
    EXPECT_EQ(stats.active_websockets.load(), 0);
    EXPECT_EQ(stats.total_websockets.load(), 0);
}

// Test WebSocket connection state management
TEST_F(WebSocketTest, ConnectionStateManagement) {
    // Create a mock socket (this would normally come from an actual connection)
    boost::asio::io_context io_context;
    boost::asio::ip::tcp::socket socket(io_context);
    
    // Test creating connection - don't use shared_ptr immediately to avoid weak_ptr issues
    {
        WebSocketConnection connection(std::move(socket));
        
        EXPECT_EQ(connection.state(), WebSocketState::CONNECTING);
        EXPECT_FALSE(connection.is_open());
        
        // Test statistics initialization
        EXPECT_EQ(connection.bytes_sent(), 0);
        EXPECT_EQ(connection.bytes_received(), 0);
        EXPECT_EQ(connection.messages_sent(), 0);
        EXPECT_EQ(connection.messages_received(), 0);
    }
    
    // No exceptions should be thrown during cleanup
    EXPECT_TRUE(true);
}

// Test frame size limits
TEST_F(WebSocketTest, FrameSizeLimits) {
    // Test small payload (< 126 bytes)
    WebSocketFrame small_frame;
    small_frame.payload.resize(100);
    small_frame.payload_length = small_frame.payload.size();
    
    auto serialized = small_frame.serialize();
    EXPECT_TRUE(serialized.size() >= 2); // At least header
    
    // Test medium payload (126-65535 bytes)
    WebSocketFrame medium_frame;
    medium_frame.payload.resize(1000);
    medium_frame.payload_length = medium_frame.payload.size();
    
    serialized = medium_frame.serialize();
    EXPECT_TRUE(serialized.size() >= 4); // Header + 2 byte length
    
    // Test large payload marker (would be > 65535 bytes)
    WebSocketFrame large_frame;
    large_frame.payload_length = 100000;
    large_frame.payload.resize(100); // Don't actually create huge payload for test
    
    serialized = large_frame.serialize();
    EXPECT_TRUE(serialized.size() >= 10); // Header + 8 byte length
}

// Test error handling in frame parsing
TEST_F(WebSocketTest, FrameParsingErrors) {
    // Test incomplete frame header
    std::vector<uint8_t> incomplete_data = {0x81}; // Only one byte
    size_t bytes_consumed = 0;
    
    EXPECT_THROW(WebSocketFrame::parse(incomplete_data, bytes_consumed), std::runtime_error);
    
    // Test incomplete payload length
    std::vector<uint8_t> incomplete_length = {0x81, 0xFE}; // 16-bit length indicator but no length bytes
    EXPECT_THROW(WebSocketFrame::parse(incomplete_length, bytes_consumed), std::runtime_error);
}

// Performance test for frame operations
TEST_F(WebSocketTest, FramePerformance) {
    const size_t num_frames = 1000;
    const std::string message = "Hello, WebSocket World!";
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (size_t i = 0; i < num_frames; ++i) {
        WebSocketFrame frame;
        frame.fin = true;
        frame.opcode = WebSocketOpcode::TEXT;
        frame.payload.assign(message.begin(), message.end());
        frame.payload_length = frame.payload.size();
        
        auto serialized = frame.serialize();
        
        size_t bytes_consumed = 0;
        auto parsed = WebSocketFrame::parse(serialized, bytes_consumed);
        
        ASSERT_EQ(parsed.payload.size(), message.size());
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    // Should be able to process 1000 frames quickly
    EXPECT_LT(duration.count(), 1000); // Less than 1 second
}

// Test WebSocket protocol compliance
TEST_F(WebSocketTest, ProtocolCompliance) {
    // Test that reserved bits are preserved
    WebSocketFrame frame;
    frame.fin = true;
    frame.rsv1 = true;  // Set reserved bit
    frame.opcode = WebSocketOpcode::TEXT;
    frame.payload = {'t', 'e', 's', 't'};
    frame.payload_length = frame.payload.size();
    
    auto serialized = frame.serialize();
    size_t bytes_consumed = 0;
    auto parsed = WebSocketFrame::parse(serialized, bytes_consumed);
    
    EXPECT_EQ(parsed.rsv1, true);
    EXPECT_EQ(parsed.rsv2, false);
    EXPECT_EQ(parsed.rsv3, false);
}

// Test concurrent WebSocket operations
TEST_F(WebSocketTest, ConcurrentOperations) {
    const size_t num_threads = 4;
    const size_t operations_per_thread = 100;
    
    std::vector<std::future<void>> futures;
    
    for (size_t t = 0; t < num_threads; ++t) {
        futures.emplace_back(std::async(std::launch::async, [&]() {
            for (size_t i = 0; i < operations_per_thread; ++i) {
                WebSocketFrame frame;
                frame.fin = true;
                frame.opcode = WebSocketOpcode::TEXT;
                frame.payload = {'t', 'e', 's', 't'};
                frame.payload_length = frame.payload.size();
                
                auto serialized = frame.serialize();
                size_t bytes_consumed = 0;
                auto parsed = WebSocketFrame::parse(serialized, bytes_consumed);
                
                ASSERT_EQ(parsed.payload, frame.payload);
            }
        }));
    }
    
    // Wait for all threads to complete
    for (auto& future : futures) {
        future.wait();
    }
}

// Test WebSocket connection cleanup
TEST_F(WebSocketTest, ConnectionCleanup) {
    boost::asio::io_context io_context;
    
    {
        boost::asio::ip::tcp::socket socket(io_context);
        auto connection = std::make_shared<WebSocketConnection>(std::move(socket));
        
        // Connection should clean up properly when going out of scope
        EXPECT_EQ(connection->state(), WebSocketState::CONNECTING);
    }
    
    // No exceptions should be thrown during cleanup
    EXPECT_TRUE(true);
}
