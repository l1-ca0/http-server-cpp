#include <gtest/gtest.h>
#include "request.hpp"

using namespace http_server;

class HttpRequestTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(HttpRequestTest, ParseSimpleGetRequest) {
    std::string raw_request = 
        "GET /path HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "User-Agent: TestClient\r\n"
        "\r\n";
    
    auto request = HttpRequest::parse(raw_request);
    
    ASSERT_TRUE(request.has_value());
    EXPECT_EQ(request->method(), HttpMethod::GET);
    EXPECT_EQ(request->path(), "/path");
    EXPECT_EQ(request->version(), "HTTP/1.1");
    EXPECT_TRUE(request->is_valid());
    
    EXPECT_TRUE(request->has_header("host"));
    EXPECT_EQ(request->get_header("host"), "example.com");
    EXPECT_EQ(request->get_header("user-agent"), "TestClient");
}

TEST_F(HttpRequestTest, ParsePostRequestWithBody) {
    std::string raw_request = 
        "POST /api/data HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: 24\r\n"
        "\r\n"
        "{\"name\":\"test\",\"id\":123}";
    
    auto request = HttpRequest::parse(raw_request);
    
    ASSERT_TRUE(request.has_value());
    EXPECT_EQ(request->method(), HttpMethod::POST);
    EXPECT_EQ(request->path(), "/api/data");
    EXPECT_EQ(request->body(), "{\"name\":\"test\",\"id\":123}");
    EXPECT_EQ(request->content_length(), 24);
    EXPECT_EQ(request->content_type(), "application/json");
}

TEST_F(HttpRequestTest, ParseRequestWithQueryParams) {
    std::string raw_request = 
        "GET /search?q=test&page=1&limit=10 HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "\r\n";
    
    auto request = HttpRequest::parse(raw_request);
    
    ASSERT_TRUE(request.has_value());
    EXPECT_EQ(request->path(), "/search");
    
    EXPECT_TRUE(request->has_query_param("q"));
    EXPECT_EQ(request->get_query_param("q"), "test");
    
    EXPECT_TRUE(request->has_query_param("page"));
    EXPECT_EQ(request->get_query_param("page"), "1");
    
    EXPECT_TRUE(request->has_query_param("limit"));
    EXPECT_EQ(request->get_query_param("limit"), "10");
    
    EXPECT_FALSE(request->has_query_param("nonexistent"));
}

TEST_F(HttpRequestTest, ParseDifferentHttpMethods) {
    struct TestCase {
        std::string method_str;
        HttpMethod expected_method;
    };
    
    std::vector<TestCase> test_cases = {
        {"GET", HttpMethod::GET},
        {"POST", HttpMethod::POST},
        {"PUT", HttpMethod::PUT},
        {"DELETE", HttpMethod::DELETE},
        {"HEAD", HttpMethod::HEAD},
        {"OPTIONS", HttpMethod::OPTIONS},
        {"PATCH", HttpMethod::PATCH}
    };
    
    for (const auto& test_case : test_cases) {
        std::string raw_request = 
            test_case.method_str + " / HTTP/1.1\r\n"
            "Host: example.com\r\n"
            "\r\n";
        
        auto request = HttpRequest::parse(raw_request);
        
        ASSERT_TRUE(request.has_value()) << "Failed to parse " << test_case.method_str;
        EXPECT_EQ(request->method(), test_case.expected_method);
    }
}

TEST_F(HttpRequestTest, MethodToStringConversion) {
    EXPECT_EQ(HttpRequest::method_to_string(HttpMethod::GET), "GET");
    EXPECT_EQ(HttpRequest::method_to_string(HttpMethod::POST), "POST");
    EXPECT_EQ(HttpRequest::method_to_string(HttpMethod::PUT), "PUT");
    EXPECT_EQ(HttpRequest::method_to_string(HttpMethod::DELETE), "DELETE");
    EXPECT_EQ(HttpRequest::method_to_string(HttpMethod::HEAD), "HEAD");
    EXPECT_EQ(HttpRequest::method_to_string(HttpMethod::OPTIONS), "OPTIONS");
    EXPECT_EQ(HttpRequest::method_to_string(HttpMethod::PATCH), "PATCH");
    EXPECT_EQ(HttpRequest::method_to_string(HttpMethod::UNKNOWN), "UNKNOWN");
}

TEST_F(HttpRequestTest, StringToMethodConversion) {
    EXPECT_EQ(HttpRequest::string_to_method("GET"), HttpMethod::GET);
    EXPECT_EQ(HttpRequest::string_to_method("POST"), HttpMethod::POST);
    EXPECT_EQ(HttpRequest::string_to_method("PUT"), HttpMethod::PUT);
    EXPECT_EQ(HttpRequest::string_to_method("DELETE"), HttpMethod::DELETE);
    EXPECT_EQ(HttpRequest::string_to_method("HEAD"), HttpMethod::HEAD);
    EXPECT_EQ(HttpRequest::string_to_method("OPTIONS"), HttpMethod::OPTIONS);
    EXPECT_EQ(HttpRequest::string_to_method("PATCH"), HttpMethod::PATCH);
    EXPECT_EQ(HttpRequest::string_to_method("INVALID"), HttpMethod::UNKNOWN);
}

TEST_F(HttpRequestTest, HeaderCaseInsensitivity) {
    std::string raw_request = 
        "GET / HTTP/1.1\r\n"
        "Content-Type: application/json\r\n"
        "content-length: 0\r\n"
        "USER-AGENT: TestClient\r\n"
        "\r\n";
    
    auto request = HttpRequest::parse(raw_request);
    
    ASSERT_TRUE(request.has_value());
    
    EXPECT_TRUE(request->has_header("content-type"));
    EXPECT_TRUE(request->has_header("content-length"));
    EXPECT_TRUE(request->has_header("user-agent"));
    
    EXPECT_EQ(request->get_header("content-type"), "application/json");
    EXPECT_EQ(request->get_header("content-length"), "0");
    EXPECT_EQ(request->get_header("user-agent"), "TestClient");
}

TEST_F(HttpRequestTest, KeepAliveDetection) {
    std::string raw_request1 = 
        "GET / HTTP/1.1\r\n"
        "Connection: keep-alive\r\n"
        "\r\n";
    
    auto request1 = HttpRequest::parse(raw_request1);
    ASSERT_TRUE(request1.has_value());
    EXPECT_TRUE(request1->is_keep_alive());
    
    std::string raw_request2 = 
        "GET / HTTP/1.1\r\n"
        "Connection: close\r\n"
        "\r\n";
    
    auto request2 = HttpRequest::parse(raw_request2);
    ASSERT_TRUE(request2.has_value());
    EXPECT_FALSE(request2->is_keep_alive());
    
    std::string raw_request3 = 
        "GET / HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "\r\n";
    
    auto request3 = HttpRequest::parse(raw_request3);
    ASSERT_TRUE(request3.has_value());
    EXPECT_TRUE(request3->is_keep_alive());
    
    std::string raw_request4 = 
        "GET / HTTP/1.0\r\n"
        "Host: example.com\r\n"
        "\r\n";
    
    auto request4 = HttpRequest::parse(raw_request4);
    ASSERT_TRUE(request4.has_value());
    EXPECT_FALSE(request4->is_keep_alive());
}

TEST_F(HttpRequestTest, InvalidRequests) {
    auto request1 = HttpRequest::parse("");
    EXPECT_FALSE(request1.has_value());
    
    std::string raw_request2 = 
        "INVALID REQUEST LINE\r\n"
        "Host: example.com\r\n"
        "\r\n";
    
    auto request2 = HttpRequest::parse(raw_request2);
    EXPECT_FALSE(request2.has_value());
    
    std::string raw_request3 = 
        "GET\r\n"
        "Host: example.com\r\n"
        "\r\n";
    
    auto request3 = HttpRequest::parse(raw_request3);
    EXPECT_FALSE(request3.has_value());
}

TEST_F(HttpRequestTest, ToStringRoundTrip) {
    std::string original_request = 
        "POST /api/test?param=value HTTP/1.1\r\n"
        "host: example.com\r\n"
        "content-type: application/json\r\n"
        "content-length: 13\r\n"
        "\r\n"
        "{\"test\":true}";
    
    auto request = HttpRequest::parse(original_request);
    ASSERT_TRUE(request.has_value());
    
    std::string reconstructed = request->to_string();
    
    auto reparsed = HttpRequest::parse(reconstructed);
    ASSERT_TRUE(reparsed.has_value());
    
    EXPECT_EQ(reparsed->method(), request->method());
    EXPECT_EQ(reparsed->path(), request->path());
    EXPECT_EQ(reparsed->version(), request->version());
    EXPECT_EQ(reparsed->body(), request->body());
    EXPECT_EQ(reparsed->query_params().size(), request->query_params().size());
}

TEST_F(HttpRequestTest, LargeHeaderValues) {
    std::string large_value(1000, 'x');
    std::string raw_request = 
        "GET / HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "Large-Header: " + large_value + "\r\n"
        "\r\n";
    
    auto request = HttpRequest::parse(raw_request);
    
    ASSERT_TRUE(request.has_value());
    EXPECT_EQ(request->get_header("large-header"), large_value);
}

TEST_F(HttpRequestTest, SpecialCharactersInPath) {
    std::string raw_request = 
        "GET /path%20with%20spaces?name=John%20Doe HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "\r\n";
    
    auto request = HttpRequest::parse(raw_request);
    
    ASSERT_TRUE(request.has_value());
    EXPECT_EQ(request->path(), "/path%20with%20spaces");
    EXPECT_EQ(request->get_query_param("name"), "John%20Doe");
}
