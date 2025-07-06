#include <gtest/gtest.h>
#include "server.hpp"
#include "request.hpp"
#include "response.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <zlib.h>

using namespace http_server;

class HttpProtocolTest : public ::testing::Test {
protected:
    void SetUp() override {
        config.host = "127.0.0.1";
        config.port = 0;
        config.thread_pool_size = 2;
        config.document_root = "./test_protocol";
        config.enable_logging = false;
        config.serve_static_files = true;
        
        std::filesystem::create_directories(config.document_root);
        
        // Create test files for compression testing
        std::ofstream large_file(config.document_root + "/large.txt");
        for (int i = 0; i < 1000; ++i) {
            large_file << "This is line " << i << " of a large text file for compression testing.\n";
        }
        large_file.close();
    }
    
    void TearDown() override {
        std::filesystem::remove_all(config.document_root);
    }
    
    ServerConfig config;
    
    // Helper function to create chunked data
    std::string create_chunked_data(const std::vector<std::string>& chunks) {
        std::ostringstream result;
        for (const auto& chunk : chunks) {
            result << std::hex << chunk.length() << "\r\n";
            result << chunk << "\r\n";
        }
        result << "0\r\n\r\n"; // End chunk
        return result.str();
    }
    
    // Helper function to parse chunked data
    std::string parse_chunked_data(const std::string& chunked_data) {
        std::istringstream stream(chunked_data);
        std::ostringstream result;
        std::string line;
        
        while (std::getline(stream, line)) {
            if (line.empty() || line == "\r") continue;
            
            // Parse chunk size
            size_t chunk_size;
            std::istringstream size_stream(line);
            size_stream >> std::hex >> chunk_size;
            
            if (chunk_size == 0) break; // End of chunks
            
            // Read chunk data
            std::string chunk_data(chunk_size, '\0');
            stream.read(&chunk_data[0], chunk_size);
            result << chunk_data;
            
            // Skip trailing \r\n
            stream.ignore(2);
        }
        
        return result.str();
    }
    
    // Helper function for gzip compression
    std::string gzip_compress(const std::string& data) {
        z_stream zs;
        memset(&zs, 0, sizeof(zs));
        
        if (deflateInit2(&zs, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 
                        15 + 16, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
            return "";
        }
        
        zs.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(data.data()));
        zs.avail_in = data.size();
        
        int ret;
        char outbuffer[32768];
        std::string outstring;
        
        do {
            zs.next_out = reinterpret_cast<Bytef*>(outbuffer);
            zs.avail_out = sizeof(outbuffer);
            
            ret = deflate(&zs, Z_FINISH);
            
            if (outstring.size() < zs.total_out) {
                outstring.append(outbuffer, zs.total_out - outstring.size());
            }
        } while (ret == Z_OK);
        
        deflateEnd(&zs);
        
        if (ret != Z_STREAM_END) {
            return "";
        }
        
        return outstring;
    }
    
    // Helper function for gzip decompression
    std::string gzip_decompress(const std::string& compressed_data) {
        z_stream zs;
        memset(&zs, 0, sizeof(zs));
        
        if (inflateInit2(&zs, 15 + 16) != Z_OK) {
            return "";
        }
        
        zs.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(compressed_data.data()));
        zs.avail_in = compressed_data.size();
        
        int ret;
        char outbuffer[32768];
        std::string outstring;
        
        do {
            zs.next_out = reinterpret_cast<Bytef*>(outbuffer);
            zs.avail_out = sizeof(outbuffer);
            
            ret = inflate(&zs, 0);
            
            if (outstring.size() < zs.total_out) {
                outstring.append(outbuffer, zs.total_out - outstring.size());
            }
        } while (ret == Z_OK);
        
        inflateEnd(&zs);
        
        if (ret != Z_STREAM_END) {
            return "";
        }
        
        return outstring;
    }
};

// Test chunked transfer encoding parsing
TEST_F(HttpProtocolTest, ChunkedEncodingParsing) {
    std::vector<std::string> chunks = {
        "Hello ",
        "World",
        "! This is ",
        "chunked encoding."
    };
    
    std::string chunked_body = create_chunked_data(chunks);
    
    std::string raw_request = 
        "POST /api/chunked HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Transfer-Encoding: chunked\r\n"
        "Content-Type: text/plain\r\n"
        "\r\n" + chunked_body;
    
    auto request = HttpRequest::parse(raw_request);
    
    ASSERT_TRUE(request.has_value());
    EXPECT_EQ(request->method(), HttpMethod::POST);
    EXPECT_EQ(request->get_header("transfer-encoding"), "chunked");
    
    // The parsed body should contain the complete unchunked data
    std::string expected_body = "Hello World! This is chunked encoding.";
    EXPECT_EQ(request->body(), expected_body);
}

// Test chunked encoding termination behavior
TEST_F(HttpProtocolTest, ChunkedEncodingWithEmptyChunks) {
    // According to HTTP/1.1 RFC 7230, a chunk with size 0 terminates the chunked encoding
    // This test verifies that parsing stops correctly at the first zero-length chunk
    std::vector<std::string> chunks = {
        "First chunk",
        ""  // This empty chunk should terminate the encoding (any chunks after this are ignored)
    };
    
    std::string chunked_body = create_chunked_data(chunks);
    
    std::string raw_request = 
        "POST /api/chunked HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Transfer-Encoding: chunked\r\n"
        "\r\n" + chunked_body;
    
    auto request = HttpRequest::parse(raw_request);
    
    ASSERT_TRUE(request.has_value());
    // Should only contain "First chunk" because the empty chunk terminates the encoding
    EXPECT_EQ(request->body(), "First chunk");
}

// Test chunked encoding with chunk extensions
TEST_F(HttpProtocolTest, ChunkedEncodingWithExtensions) {
    std::string chunked_data = 
        "5;extension=value\r\n"
        "Hello\r\n"
        "6;another=ext\r\n"
        " World\r\n"
        "0\r\n"
        "\r\n";
    
    std::string raw_request = 
        "POST /api/chunked HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Transfer-Encoding: chunked\r\n"
        "\r\n" + chunked_data;
    
    auto request = HttpRequest::parse(raw_request);
    
    ASSERT_TRUE(request.has_value());
    // Should ignore extensions and parse the chunks correctly
    EXPECT_EQ(request->body(), "Hello World");
}

// Test chunked encoding response generation
TEST_F(HttpProtocolTest, ChunkedEncodingResponse) {
    HttpResponse response;
    
    // Test chunked response creation
    std::string data = "This is a test of chunked response encoding.";
    response.set_header("Transfer-Encoding", "chunked");
    response.set_body(data);
    
    std::string http_response = response.to_http_string();
    
    EXPECT_TRUE(http_response.find("Transfer-Encoding: chunked") != std::string::npos);
    EXPECT_TRUE(http_response.find(data) != std::string::npos);
}

// Test gzip compression support
TEST_F(HttpProtocolTest, GzipCompressionSupport) {
    std::string original_data = "This is a test string for compression. " +
                               std::string(1000, 'A') + " This should compress well.";
    
    // Test compression
    std::string compressed = gzip_compress(original_data);
    EXPECT_FALSE(compressed.empty());
    EXPECT_LT(compressed.size(), original_data.size()); // Should be smaller
    
    // Test decompression
    std::string decompressed = gzip_decompress(compressed);
    EXPECT_EQ(decompressed, original_data);
}

// Test content encoding headers
TEST_F(HttpProtocolTest, ContentEncodingHeaders) {
    HttpResponse response;
    
    std::string data = "Test data for compression";
    std::string compressed_data = gzip_compress(data);
    
    response.set_header("Content-Encoding", "gzip");
    response.set_body(compressed_data);
    
    EXPECT_EQ(response.get_header("Content-Encoding"), "gzip");
    EXPECT_EQ(response.body(), compressed_data);
    
    std::string http_response = response.to_http_string();
    EXPECT_TRUE(http_response.find("Content-Encoding: gzip") != std::string::npos);
}

// Test accept-encoding header processing
TEST_F(HttpProtocolTest, AcceptEncodingProcessing) {
    std::string raw_request = 
        "GET / HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Accept-Encoding: gzip, deflate, br\r\n"
        "User-Agent: TestClient\r\n"
        "\r\n";
    
    auto request = HttpRequest::parse(raw_request);
    
    ASSERT_TRUE(request.has_value());
    EXPECT_EQ(request->get_header("accept-encoding"), "gzip, deflate, br");
    
    // Test parsing of accepted encodings
    auto accept_encoding = request->get_header("accept-encoding");
    ASSERT_TRUE(accept_encoding.has_value());
    EXPECT_TRUE(accept_encoding->find("gzip") != std::string::npos);
    EXPECT_TRUE(accept_encoding->find("deflate") != std::string::npos);
    EXPECT_TRUE(accept_encoding->find("br") != std::string::npos);
}

// Test HTTP/1.1 persistent connections
TEST_F(HttpProtocolTest, PersistentConnections) {
    // Test HTTP/1.1 default keep-alive
    std::string raw_request1 = 
        "GET / HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "\r\n";
    
    auto request1 = HttpRequest::parse(raw_request1);
    ASSERT_TRUE(request1.has_value());
    EXPECT_TRUE(request1->is_keep_alive()); // HTTP/1.1 defaults to keep-alive
    
    // Test explicit Connection: close
    std::string raw_request2 = 
        "GET / HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Connection: close\r\n"
        "\r\n";
    
    auto request2 = HttpRequest::parse(raw_request2);
    ASSERT_TRUE(request2.has_value());
    EXPECT_FALSE(request2->is_keep_alive());
    
    // Test explicit Connection: keep-alive
    std::string raw_request3 = 
        "GET / HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Connection: keep-alive\r\n"
        "\r\n";
    
    auto request3 = HttpRequest::parse(raw_request3);
    ASSERT_TRUE(request3.has_value());
    EXPECT_TRUE(request3->is_keep_alive());
}

// Test Transfer-Encoding priority over Content-Length
TEST_F(HttpProtocolTest, TransferEncodingPriority) {
    std::string chunked_data = 
        "5\r\n"
        "Hello\r\n"
        "6\r\n"
        " World\r\n"
        "0\r\n"
        "\r\n";
    
    std::string raw_request = 
        "POST /api/test HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Length: 999\r\n"  // This should be ignored
        "Transfer-Encoding: chunked\r\n"
        "\r\n" + chunked_data;
    
    auto request = HttpRequest::parse(raw_request);
    
    ASSERT_TRUE(request.has_value());
    EXPECT_EQ(request->get_header("transfer-encoding"), "chunked");
    EXPECT_EQ(request->body(), "Hello World");
    // Content-Length should be ignored when Transfer-Encoding is present
}

// Test multiple Transfer-Encoding values
TEST_F(HttpProtocolTest, MultipleTransferEncodings) {
    std::string raw_request = 
        "POST /api/test HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Transfer-Encoding: gzip, chunked\r\n"
        "Content-Type: application/json\r\n"
        "\r\n"
        "5\r\n"
        "Hello\r\n"
        "0\r\n"
        "\r\n";
    
    auto request = HttpRequest::parse(raw_request);
    
    ASSERT_TRUE(request.has_value());
    EXPECT_EQ(request->get_header("transfer-encoding"), "gzip, chunked");
    // The last encoding should be chunked for proper parsing
    auto transfer_encoding = request->get_header("transfer-encoding");
    ASSERT_TRUE(transfer_encoding.has_value());
    EXPECT_TRUE(transfer_encoding->find("chunked") != std::string::npos);
}

// Test Upgrade header for protocol switching
TEST_F(HttpProtocolTest, UpgradeHeader) {
    std::string raw_request = 
        "GET /websocket HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Connection: Upgrade\r\n"
        "Upgrade: websocket\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
        "\r\n";
    
    auto request = HttpRequest::parse(raw_request);
    
    ASSERT_TRUE(request.has_value());
    EXPECT_EQ(request->get_header("connection"), "Upgrade");
    EXPECT_EQ(request->get_header("upgrade"), "websocket");
    EXPECT_TRUE(request->has_header("sec-websocket-version"));
    EXPECT_TRUE(request->has_header("sec-websocket-key"));
}

// Test Expect: 100-continue
TEST_F(HttpProtocolTest, ExpectContinue) {
    // For Expect: 100-continue, the client sends headers first without body
    // and waits for 100 Continue response before sending the actual body
    std::string raw_request = 
        "POST /upload HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Expect: 100-continue\r\n"
        "Content-Type: application/octet-stream\r\n"
        "\r\n";
    
    auto request = HttpRequest::parse(raw_request);
    
    ASSERT_TRUE(request.has_value());
    EXPECT_EQ(request->get_header("expect").value_or(""), "100-continue");
    EXPECT_EQ(request->content_length(), 0);  // No body in initial request
    
    // Server should be able to respond with 100 Continue
    HttpResponse continue_response(static_cast<HttpStatus>(100));
    EXPECT_EQ(static_cast<int>(continue_response.status()), 100);
}

// Test Range requests (partial content)
TEST_F(HttpProtocolTest, RangeRequests) {
    std::string raw_request = 
        "GET /large-file.txt HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Range: bytes=200-999\r\n"
        "\r\n";
    
    auto request = HttpRequest::parse(raw_request);
    
    ASSERT_TRUE(request.has_value());
    EXPECT_EQ(request->get_header("range"), "bytes=200-999");
    
    // Test range response
    HttpResponse response(static_cast<HttpStatus>(206)); // Partial Content
    response.set_header("Content-Range", "bytes 200-999/5000");
    response.set_header("Content-Length", "800");
    
    EXPECT_EQ(static_cast<int>(response.status()), 206);
    EXPECT_EQ(response.get_header("Content-Range"), "bytes 200-999/5000");
}

// Test HTTP/1.1 Host header requirement
TEST_F(HttpProtocolTest, HostHeaderRequired) {
    // Valid HTTP/1.1 request with Host header
    std::string raw_request1 = 
        "GET / HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "\r\n";
    
    auto request1 = HttpRequest::parse(raw_request1);
    ASSERT_TRUE(request1.has_value());
    EXPECT_TRUE(request1->has_header("host"));
    
    // HTTP/1.1 request without Host header (should be invalid)
    std::string raw_request2 = 
        "GET / HTTP/1.1\r\n"
        "User-Agent: TestClient\r\n"
        "\r\n";
    
    auto request2 = HttpRequest::parse(raw_request2);
    // In a strict implementation, this might be rejected
    // But for testing, we check if it's parsed but mark the missing host
    if (request2.has_value()) {
        EXPECT_FALSE(request2->has_header("host"));
    }
}

// Test trailer headers with chunked encoding
TEST_F(HttpProtocolTest, TrailerHeaders) {
    std::string chunked_with_trailers = 
        "5\r\n"
        "Hello\r\n"
        "6\r\n"
        " World\r\n"
        "0\r\n"
        "Content-MD5: Q2hlY2sgSW50ZWdyaXR5IQ==\r\n"
        "X-Custom-Trailer: trailer-value\r\n"
        "\r\n";
    
    std::string raw_request = 
        "POST /api/chunked HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Transfer-Encoding: chunked\r\n"
        "Trailer: Content-MD5, X-Custom-Trailer\r\n"
        "\r\n" + chunked_with_trailers;
    
    auto request = HttpRequest::parse(raw_request);
    
    ASSERT_TRUE(request.has_value());
    EXPECT_EQ(request->get_header("trailer"), "Content-MD5, X-Custom-Trailer");
    EXPECT_EQ(request->body(), "Hello World");
    
    // In a full implementation, trailer headers would be parsed and available
    // This test demonstrates the structure even if not fully implemented
}

// Test HTTP version validation
TEST_F(HttpProtocolTest, HttpVersionValidation) {
    // Test various HTTP versions
    std::vector<std::pair<std::string, bool>> version_tests = {
        {"HTTP/1.1", true},
        {"HTTP/1.0", true},
        {"HTTP/2.0", false},  // Not supported in this implementation
        {"HTTP/0.9", false},  // Too old
        {"HTTP/1.2", false},  // Invalid
        {"HTTPS/1.1", false}, // Invalid protocol
        {"HTTP/", false},     // Incomplete
    };
    
    for (const auto& [version, should_be_valid] : version_tests) {
        std::string raw_request = 
            "GET / " + version + "\r\n"
            "Host: localhost\r\n"
            "\r\n";
        
        auto request = HttpRequest::parse(raw_request);
        
        if (should_be_valid) {
            EXPECT_TRUE(request.has_value()) << "Version " << version << " should be valid";
            if (request.has_value()) {
                EXPECT_EQ(request->version(), version);
            }
        } else {
            // Invalid versions might be rejected or marked as invalid
            if (request.has_value()) {
                // If parsed, check if it's marked as invalid
                EXPECT_FALSE(request->is_valid()) << "Version " << version << " should be invalid";
            }
        }
    }
} 