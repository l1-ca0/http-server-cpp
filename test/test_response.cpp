#include <gtest/gtest.h>
#include "response.hpp"
#include <filesystem>
#include <fstream>
#include <string>
#include <sstream>

using namespace http_server;

class HttpResponseTest : public ::testing::Test {
protected:
    void SetUp() override {
        std::filesystem::create_directories("test_files");
        
        std::ofstream text_file("test_files/test.txt");
        text_file << "Hello, World!";
        text_file.close();
        
        std::ofstream html_file("test_files/test.html");
        html_file << "<html><body><h1>Test</h1></body></html>";
        html_file.close();
        
        std::ofstream json_file("test_files/test.json");
        json_file << "{\"message\":\"test\"}";
        json_file.close();
    }
    
    void TearDown() override {
        std::filesystem::remove_all("test_files");
    }
};

TEST_F(HttpResponseTest, DefaultConstructor) {
    HttpResponse response;
    
    EXPECT_EQ(response.status(), HttpStatus::OK);
    EXPECT_TRUE(response.has_header("Server"));
    EXPECT_TRUE(response.has_header("Date"));
    EXPECT_TRUE(response.has_header("Content-Length"));
    EXPECT_EQ(response.get_header("Content-Length"), "0");
}

TEST_F(HttpResponseTest, StatusConstructor) {
    HttpResponse response(HttpStatus::NOT_FOUND);
    
    EXPECT_EQ(response.status(), HttpStatus::NOT_FOUND);
}

TEST_F(HttpResponseTest, SetAndGetStatus) {
    HttpResponse response;
    
    response.set_status(HttpStatus::CREATED);
    EXPECT_EQ(response.status(), HttpStatus::CREATED);
    
    response.set_status(HttpStatus::BAD_REQUEST);
    EXPECT_EQ(response.status(), HttpStatus::BAD_REQUEST);
}

TEST_F(HttpResponseTest, HeaderManagement) {
    HttpResponse response;
    
    response.set_header("Custom-Header", "test-value");
    EXPECT_TRUE(response.has_header("Custom-Header"));
    EXPECT_EQ(response.get_header("Custom-Header"), "test-value");
    
    response.add_header("Custom-Header", "second-value");
    EXPECT_EQ(response.get_header("Custom-Header"), "test-value, second-value");
    
    response.remove_header("Custom-Header");
    EXPECT_FALSE(response.has_header("Custom-Header"));
    EXPECT_EQ(response.get_header("Custom-Header"), "");
}

TEST_F(HttpResponseTest, HeaderCaseNormalization) {
    HttpResponse response;
    
    response.set_header("content-type", "application/json");
    
    EXPECT_TRUE(response.has_header("Content-Type"));
    EXPECT_EQ(response.get_header("Content-Type"), "application/json");
}

TEST_F(HttpResponseTest, BodyManagement) {
    HttpResponse response;
    
    std::string test_body = "Test body content";
    response.set_body(test_body);
    
    EXPECT_EQ(response.body(), test_body);
    EXPECT_EQ(response.get_header("Content-Length"), std::to_string(test_body.length()));
    
    std::string move_body = "Move body content";
    response.set_body(std::move(move_body));
    EXPECT_EQ(response.body(), "Move body content");
}

TEST_F(HttpResponseTest, ContentTypeHelpers) {
    HttpResponse response;
    
    response.set_json("{\"test\":true}");
    EXPECT_EQ(response.get_header("Content-Type"), "application/json; charset=utf-8");
    EXPECT_EQ(response.body(), "{\"test\":true}");
    
    response.set_html("<h1>Test</h1>");
    EXPECT_EQ(response.get_header("Content-Type"), "text/html; charset=utf-8");
    EXPECT_EQ(response.body(), "<h1>Test</h1>");
    
    response.set_text("Plain text");
    EXPECT_EQ(response.get_header("Content-Type"), "text/plain; charset=utf-8");
    EXPECT_EQ(response.body(), "Plain text");
}

TEST_F(HttpResponseTest, FileContent) {
    HttpResponse response;
    
    response.set_file_content("test_files/test.txt");
    EXPECT_EQ(response.status(), HttpStatus::OK);
    EXPECT_EQ(response.body(), "Hello, World!");
    EXPECT_EQ(response.get_header("Content-Type"), "text/plain");
    
    response.set_file_content("test_files/test.html");
    EXPECT_EQ(response.body(), "<html><body><h1>Test</h1></body></html>");
    EXPECT_EQ(response.get_header("Content-Type"), "text/html");
    
    response.set_file_content("test_files/nonexistent.txt");
    EXPECT_EQ(response.status(), HttpStatus::NOT_FOUND);
    EXPECT_EQ(response.body(), "File not found");
}

TEST_F(HttpResponseTest, SpecialHeaders) {
    HttpResponse response;
    
    response.set_keep_alive(true);
    EXPECT_EQ(response.get_header("Connection"), "keep-alive");
    
    response.set_keep_alive(false);
    EXPECT_EQ(response.get_header("Connection"), "close");
    
    response.set_cache_control("no-cache");
    EXPECT_EQ(response.get_header("Cache-Control"), "no-cache");
    
    response.set_cors_headers("https://example.com");
    EXPECT_EQ(response.get_header("Access-Control-Allow-Origin"), "https://example.com");
    EXPECT_TRUE(response.has_header("Access-Control-Allow-Methods"));
    EXPECT_TRUE(response.has_header("Access-Control-Allow-Headers"));
}

TEST_F(HttpResponseTest, StaticFactoryMethods) {
    auto ok_response = HttpResponse::ok("Success");
    EXPECT_EQ(ok_response.status(), HttpStatus::OK);
    EXPECT_EQ(ok_response.body(), "Success");
    
    auto not_found = HttpResponse::not_found("Resource not available");
    EXPECT_EQ(not_found.status(), HttpStatus::NOT_FOUND);
    EXPECT_EQ(not_found.body(), "Resource not available");
    
    auto bad_request = HttpResponse::bad_request("Invalid input");
    EXPECT_EQ(bad_request.status(), HttpStatus::BAD_REQUEST);
    EXPECT_EQ(bad_request.body(), "Invalid input");
    
    auto internal_error = HttpResponse::internal_error("Server error");
    EXPECT_EQ(internal_error.status(), HttpStatus::INTERNAL_SERVER_ERROR);
    EXPECT_EQ(internal_error.body(), "Server error");
    
    auto json_response = HttpResponse::json_response("{\"success\":true}", HttpStatus::CREATED);
    EXPECT_EQ(json_response.status(), HttpStatus::CREATED);
    EXPECT_EQ(json_response.get_header("Content-Type"), "application/json; charset=utf-8");
    EXPECT_EQ(json_response.body(), "{\"success\":true}");
    
    auto file_response = HttpResponse::file_response("test_files/test.txt");
    EXPECT_EQ(file_response.status(), HttpStatus::OK);
    EXPECT_EQ(file_response.body(), "Hello, World!");
}

TEST_F(HttpResponseTest, MimeTypeDetection) {
    struct TestCase {
        std::string extension;
        std::string expected_mime;
    };
    
    std::vector<TestCase> test_cases = {
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
        {"pdf", "application/pdf"},
        {"zip", "application/zip"},
        {"mp4", "video/mp4"},
        {"mp3", "audio/mpeg"},
        {"unknown", "application/octet-stream"}
    };
    
    for (const auto& test_case : test_cases) {
        EXPECT_EQ(HttpResponse::get_mime_type(test_case.extension), test_case.expected_mime)
            << "Failed for extension: " << test_case.extension;
    }
}

TEST_F(HttpResponseTest, StatusMessages) {
    struct TestCase {
        HttpStatus status;
        std::string expected_message;
    };
    
    std::vector<TestCase> test_cases = {
        {HttpStatus::OK, "OK"},
        {HttpStatus::CREATED, "Created"},
        {HttpStatus::BAD_REQUEST, "Bad Request"},
        {HttpStatus::UNAUTHORIZED, "Unauthorized"},
        {HttpStatus::FORBIDDEN, "Forbidden"},
        {HttpStatus::NOT_FOUND, "Not Found"},
        {HttpStatus::METHOD_NOT_ALLOWED, "Method Not Allowed"},
        {HttpStatus::INTERNAL_SERVER_ERROR, "Internal Server Error"},
        {HttpStatus::NOT_IMPLEMENTED, "Not Implemented"},
        {HttpStatus::SERVICE_UNAVAILABLE, "Service Unavailable"}
    };
    
    for (const auto& test_case : test_cases) {
        EXPECT_EQ(HttpResponse::get_status_message(test_case.status), test_case.expected_message)
            << "Failed for status: " << static_cast<int>(test_case.status);
    }
}

TEST_F(HttpResponseTest, HttpStringGeneration) {
    HttpResponse response(HttpStatus::OK);
    response.set_text("Hello, World!");
    response.set_header("Custom-Header", "custom-value");
    
    std::string http_string = response.to_http_string();
    
    EXPECT_TRUE(http_string.starts_with("HTTP/1.1 200 OK"));
    
    EXPECT_TRUE(http_string.find("Content-Type: text/plain; charset=utf-8") != std::string::npos);
    EXPECT_TRUE(http_string.find("Custom-Header: custom-value") != std::string::npos);
    EXPECT_TRUE(http_string.find("Content-Length: 13") != std::string::npos);
    
    EXPECT_TRUE(http_string.ends_with("Hello, World!"));
    
    EXPECT_TRUE(http_string.find("\r\n\r\n") != std::string::npos);
}

TEST_F(HttpResponseTest, ToStringDebugOutput) {
    HttpResponse response(HttpStatus::NOT_FOUND);
    response.set_text("Page not found");
    response.set_header("Custom-Header", "debug-value");
    
    std::string debug_string = response.to_string();
    
    EXPECT_TRUE(debug_string.find("Status: 404 Not Found") != std::string::npos);
    
    EXPECT_TRUE(debug_string.find("Custom-Header: debug-value") != std::string::npos);
    
    EXPECT_TRUE(debug_string.find("Body (14 bytes):") != std::string::npos);
    
    EXPECT_TRUE(debug_string.find("Page not found") != std::string::npos);
}

TEST_F(HttpResponseTest, FluentInterface) {
    auto response = HttpResponse()
        .set_status(HttpStatus::CREATED)
        .set_header("Location", "/new-resource")
        .set_json("{\"id\":123}")
        .set_keep_alive(true)
        .set_cors_headers();
    
    EXPECT_EQ(response.status(), HttpStatus::CREATED);
    EXPECT_EQ(response.get_header("Location"), "/new-resource");
    EXPECT_EQ(response.get_header("Content-Type"), "application/json; charset=utf-8");
    EXPECT_EQ(response.get_header("Connection"), "keep-alive");
    EXPECT_TRUE(response.has_header("Access-Control-Allow-Origin"));
    EXPECT_EQ(response.body(), "{\"id\":123}");
}

TEST_F(HttpResponseTest, LargeBodyHandling) {
    HttpResponse response;
    
    std::string large_body(1024 * 1024, 'x');
    response.set_body(large_body);
    
    EXPECT_EQ(response.body().size(), 1024 * 1024);
    EXPECT_EQ(response.get_header("Content-Length"), std::to_string(1024 * 1024));
    
    std::string http_string = response.to_http_string();
    EXPECT_GT(http_string.size(), 1024 * 1024);
}

TEST_F(HttpResponseTest, EmptyBodyHandling) {
    HttpResponse response;
    
    EXPECT_EQ(response.body(), "");
    EXPECT_EQ(response.get_header("Content-Length"), "0");
    
    response.set_body("");
    EXPECT_EQ(response.body(), "");
    EXPECT_EQ(response.get_header("Content-Length"), "0");
}
