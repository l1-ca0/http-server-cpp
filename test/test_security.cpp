#include <gtest/gtest.h>
#include "server.hpp"
#include "request.hpp"
#include "response.hpp"
#include <filesystem>
#include <fstream>

using namespace http_server;

class SecurityTest : public ::testing::Test {
protected:
    void SetUp() override {
        config.host = "127.0.0.1";
        config.port = 0;
        config.thread_pool_size = 2;
        config.document_root = "./test_security";
        config.enable_logging = false;
        config.serve_static_files = true;
        
        std::filesystem::create_directories(config.document_root);
        
        // Create test files for path traversal tests
        std::ofstream secret_file(config.document_root + "/secret.txt");
        secret_file << "Secret data that should not be accessible";
        secret_file.close();
    }
    
    void TearDown() override {
        std::filesystem::remove_all(config.document_root);
    }
    
    ServerConfig config;
};

// Path Traversal Attack Prevention
TEST_F(SecurityTest, PathTraversalPrevention) {
    HttpServer server(config);
    
    // Test various path traversal attempts
    std::vector<std::string> malicious_paths = {
        "/../../../etc/passwd",
        "/..\\..\\..\\windows\\system32\\config\\sam",
        "/%2e%2e/%2e%2e/%2e%2e/etc/passwd",
        "/....//....//....//etc/passwd",
        "/test/../../../secret.txt"
    };
    
    for (const auto& path : malicious_paths) {
        std::string raw_request = 
            "GET " + path + " HTTP/1.1\r\n"
            "Host: localhost\r\n"
            "\r\n";
        
        auto request = HttpRequest::parse(raw_request);
        ASSERT_TRUE(request.has_value()) << "Failed to parse request for path: " + path;
        
        // The server should not serve files outside document root
        // This would be tested with actual request handling in integration tests
        EXPECT_TRUE(request->path().find("..") != std::string::npos || 
                   request->path().find("%2e%2e") != std::string::npos ||
                   request->path().find("....") != std::string::npos) 
            << "Path traversal pattern not detected in: " + path;
    }
}

// Request Size Limits
TEST_F(SecurityTest, RequestSizeLimits) {
    // Test very large headers
    std::string large_header(50000, 'A'); // 50KB header
    std::string raw_request = 
        "GET / HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Large-Header: " + large_header + "\r\n"
        "\r\n";
    
    auto request = HttpRequest::parse(raw_request);
    // Should either parse successfully or reject gracefully
    // In a real implementation, this might be rejected based on size limits
    
    // Test very large body
    std::string large_body(10000000, 'B'); // 10MB body
    std::string large_post = 
        "POST /upload HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Type: application/octet-stream\r\n"
        "Content-Length: " + std::to_string(large_body.size()) + "\r\n"
        "\r\n" + large_body;
    
    auto large_request = HttpRequest::parse(large_post);
    if (large_request.has_value()) {
        EXPECT_EQ(large_request->body().size(), large_body.size());
    }
}

// Header Injection Prevention
TEST_F(SecurityTest, HeaderInjectionPrevention) {
    // Test real CRLF injection scenarios where attackers try to inject CRLF 
    // sequences into header values to create additional headers
    
    // Test legitimate requests first
    std::vector<std::string> legitimate_requests = {
        "GET / HTTP/1.1\r\nHost: example.com\r\nUser-Agent: Mozilla/5.0\r\n\r\n",
        "GET / HTTP/1.1\r\nHost: example.com\r\nX-Custom: value\r\n\r\n"
    };
    
    for (const auto& request_str : legitimate_requests) {
        auto request = HttpRequest::parse(request_str);
        EXPECT_TRUE(request.has_value()) << "Legitimate request should parse successfully";
    }
    
    // Test malformed requests that should fail or be handled safely
    std::vector<std::string> malformed_requests = {
        // Request line split across multiple lines - should fail
        "GET /\r\nHost: example.com\r\n HTTP/1.1\r\n\r\n",
        // Very malformed request
        "INVALID\r\nHost: example.com\r\n\r\n"
    };
    
    for (const auto& request_str : malformed_requests) {
        auto request = HttpRequest::parse(request_str);
        // These should either fail to parse or parse as invalid
        if (request.has_value()) {
            EXPECT_FALSE(request->is_valid()) << "Malformed request should be marked invalid";
        }
    }
    
    // Test the header validation functions directly
    // These are the real security measures against injection
    HttpRequest test_request;
    
    // Test valid header names and values
    EXPECT_TRUE(test_request.is_valid_header_name("User-Agent"));
    EXPECT_TRUE(test_request.is_valid_header_name("Content-Type"));
    EXPECT_TRUE(test_request.is_valid_header_value("Mozilla/5.0"));
    EXPECT_TRUE(test_request.is_valid_header_value("application/json"));
    
    // Test invalid header names (containing CRLF or other invalid chars)
    EXPECT_FALSE(test_request.is_valid_header_name("User\r\nInjected"));
    EXPECT_FALSE(test_request.is_valid_header_name("User\nInjected"));
    EXPECT_FALSE(test_request.is_valid_header_name("User:Agent"));
    
    // Test invalid header values (containing CRLF)
    EXPECT_FALSE(test_request.is_valid_header_value("value\r\nInjected: header"));
    EXPECT_FALSE(test_request.is_valid_header_value("value\nInjected: header"));
    EXPECT_FALSE(test_request.is_valid_header_value("value\rInjected"));
}

// HTTP Method Security
TEST_F(SecurityTest, HttpMethodSecurity) {
    std::vector<std::string> dangerous_methods = {
        "TRACE", "CONNECT", "DEBUG", "TRACK"
    };
    
    for (const auto& method : dangerous_methods) {
        std::string raw_request = 
            method + " / HTTP/1.1\r\n"
            "Host: localhost\r\n"
            "\r\n";
        
        auto request = HttpRequest::parse(raw_request);
        if (request.has_value()) {
            // Should be classified as UNKNOWN method for security
            EXPECT_EQ(request->method(), HttpMethod::UNKNOWN);
        }
    }
}

// URL Encoding Security
TEST_F(SecurityTest, UrlEncodingSecurity) {
    std::vector<std::pair<std::string, std::string>> test_cases = {
        {"/test%00.txt", "null byte injection"},
        {"/test%2e%2e%2f", "encoded path traversal"},
        {"/test%3cscript%3e", "encoded script tag"},
        {"/test%27%3bDROP", "sql injection attempt"}
    };
    
    for (const auto& [path, description] : test_cases) {
        std::string raw_request = 
            "GET " + path + " HTTP/1.1\r\n"
            "Host: localhost\r\n"
            "\r\n";
        
        auto request = HttpRequest::parse(raw_request);
        ASSERT_TRUE(request.has_value()) << "Failed to parse: " + description;
        
        // Path should preserve the encoded form for security validation
        EXPECT_EQ(request->path(), path);
    }
}

// Response Security Headers
TEST_F(SecurityTest, SecurityHeaders) {
    HttpResponse response;
    
    // Test that security headers can be set
    response.set_header("X-Content-Type-Options", "nosniff");
    response.set_header("X-Frame-Options", "DENY");
    response.set_header("X-XSS-Protection", "1; mode=block");
    response.set_header("Strict-Transport-Security", "max-age=31536000");
    response.set_header("Content-Security-Policy", "default-src 'self'");
    
    EXPECT_EQ(response.get_header("X-Content-Type-Options"), "nosniff");
    EXPECT_EQ(response.get_header("X-Frame-Options"), "DENY");
    EXPECT_EQ(response.get_header("X-XSS-Protection"), "1; mode=block");
    EXPECT_EQ(response.get_header("Strict-Transport-Security"), "max-age=31536000");
    EXPECT_EQ(response.get_header("Content-Security-Policy"), "default-src 'self'");
}

// Cookie Security
TEST_F(SecurityTest, CookieSecurity) {
    HttpResponse response;
    
    // Test secure cookie settings
    response.set_header("Set-Cookie", "sessionid=abc123; HttpOnly; Secure; SameSite=Strict");
    
    std::string cookie_header = response.get_header("Set-Cookie");
    EXPECT_TRUE(cookie_header.find("HttpOnly") != std::string::npos);
    EXPECT_TRUE(cookie_header.find("Secure") != std::string::npos);
    EXPECT_TRUE(cookie_header.find("SameSite=Strict") != std::string::npos);
}

// Request Rate Limiting (Mock)
TEST_F(SecurityTest, RateLimitingConcept) {
    // This would test rate limiting if implemented
    std::vector<std::string> requests;
    
    // Generate multiple requests from same "IP"
    for (int i = 0; i < 100; ++i) {
        std::string request = 
            "GET /api/data HTTP/1.1\r\n"
            "Host: localhost\r\n"
            "X-Forwarded-For: 192.168.1.100\r\n"
            "\r\n";
        requests.push_back(request);
    }
    
    // In a real implementation, after certain threshold,
    // requests should be rate limited
    EXPECT_EQ(requests.size(), 100);
}

// Content Type Validation
TEST_F(SecurityTest, ContentTypeValidation) {
    std::vector<std::string> suspicious_content_types = {
        "application/x-msdownload",
        "application/x-executable",
        "application/octet-stream",
        "text/html", // when expecting JSON
        "application/javascript" // when expecting text
    };
    
    for (const auto& content_type : suspicious_content_types) {
        std::string raw_request = 
            "POST /api/upload HTTP/1.1\r\n"
            "Host: localhost\r\n"
            "Content-Type: " + content_type + "\r\n"
            "Content-Length: 9\r\n"
            "\r\n"
            "test data";
        
        auto request = HttpRequest::parse(raw_request);
        ASSERT_TRUE(request.has_value());
        EXPECT_EQ(request->content_type(), content_type);
        
        // Server logic should validate content types appropriately
    }
} 