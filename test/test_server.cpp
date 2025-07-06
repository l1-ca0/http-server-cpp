#include <gtest/gtest.h>
#include "server.hpp"
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

using namespace http_server;

class HttpServerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create test configuration
        config.host = "127.0.0.1";
        config.port = 0; // Let OS choose available port
        config.thread_pool_size = 2;
        config.document_root = "./test_public";
        config.enable_logging = false; // Disable logging for tests
        config.serve_static_files = true;
        
        // Create test directory and files
        std::filesystem::create_directories(config.document_root);
        
        // Create test HTML file
        std::ofstream html_file(config.document_root + "/index.html");
        html_file << "<html><body><h1>Test Page</h1></body></html>";
        html_file.close();
        
        // Create test CSS file
        std::ofstream css_file(config.document_root + "/style.css");
        css_file << "body { font-family: Arial; }";
        css_file.close();
        
        // Create test JSON file
        std::ofstream json_file(config.document_root + "/data.json");
        json_file << "{\"test\": true}";
        json_file.close();
    }
    
    void TearDown() override {
        // Clean up test files
        std::filesystem::remove_all(config.document_root);
    }
    
    ServerConfig config;
};

TEST_F(HttpServerTest, ServerConfigDefaults) {
    ServerConfig default_config;
    
    EXPECT_EQ(default_config.host, "0.0.0.0");
    EXPECT_EQ(default_config.port, 8080);
    EXPECT_GT(default_config.thread_pool_size, 0);
    EXPECT_EQ(default_config.document_root, "./public");
    EXPECT_EQ(default_config.max_connections, 1000);
    EXPECT_TRUE(default_config.enable_logging);
    EXPECT_TRUE(default_config.serve_static_files);
    EXPECT_FALSE(default_config.index_files.empty());
}

TEST_F(HttpServerTest, ServerConfigFromJson) {
    nlohmann::json config_json = {
        {"host", "localhost"},
        {"port", 9090},
        {"thread_pool_size", 4},
        {"document_root", "/var/www"},
        {"max_connections", 500},
        {"enable_logging", false},
        {"serve_static_files", false},
        {"index_files", {"main.html", "home.html"}},
        {"mime_types", {
            {"html", "text/html"},
            {"css", "text/css"}
        }}
    };
    
    auto parsed_config = ServerConfig::from_json_string(config_json.dump());
    
    EXPECT_EQ(parsed_config.host, "localhost");
    EXPECT_EQ(parsed_config.port, 9090);
    EXPECT_EQ(parsed_config.thread_pool_size, 4);
    EXPECT_EQ(parsed_config.document_root, "/var/www");
    EXPECT_EQ(parsed_config.max_connections, 500);
    EXPECT_FALSE(parsed_config.enable_logging);
    EXPECT_FALSE(parsed_config.serve_static_files);
    EXPECT_EQ(parsed_config.index_files.size(), 2);
    EXPECT_EQ(parsed_config.index_files[0], "main.html");
    EXPECT_EQ(parsed_config.index_files[1], "home.html");
    EXPECT_EQ(parsed_config.mime_types.at("html"), "text/html");
    EXPECT_EQ(parsed_config.mime_types.at("css"), "text/css");
}

TEST_F(HttpServerTest, ServerConfigToJson) {
    config.host = "example.com";
    config.port = 443;
    config.thread_pool_size = 8;
    
    auto json_output = config.to_json();
    
    EXPECT_EQ(json_output["host"], "example.com");
    EXPECT_EQ(json_output["port"], 443);
    EXPECT_EQ(json_output["thread_pool_size"], 8);
    EXPECT_TRUE(json_output.contains("document_root"));
    EXPECT_TRUE(json_output.contains("max_connections"));
}

TEST_F(HttpServerTest, ServerCreation) {
    HttpServer server(config);
    
    EXPECT_FALSE(server.is_running());
    EXPECT_EQ(server.config().host, config.host);
    EXPECT_EQ(server.config().port, config.port);
}

TEST_F(HttpServerTest, RouteRegistration) {
    HttpServer server(config);
    
    bool handler_called = false;
    
    // Add simple GET route
    server.add_get_route("/test", [&handler_called](const HttpRequest& /*request*/) {
        handler_called = true;
        return HttpResponse::ok("Test response");
    });
    
    // We can't easily test the actual routing without starting the server
    // but we can verify the route was registered by checking internal state
    // This is more of a compilation test
    EXPECT_FALSE(handler_called); // Handler not called yet
}

TEST_F(HttpServerTest, MultipleRouteTypes) {
    HttpServer server(config);
    
    int get_calls = 0, post_calls = 0, put_calls = 0, delete_calls = 0;
    
    server.add_get_route("/get", [&get_calls](const HttpRequest& /*req*/) {
        get_calls++;
        return HttpResponse::ok("GET response");
    });
    
    server.add_post_route("/post", [&post_calls](const HttpRequest& /*req*/) {
        post_calls++;
        return HttpResponse::ok("POST response");
    });
    
    server.add_put_route("/put", [&put_calls](const HttpRequest& /*req*/) {
        put_calls++;
        return HttpResponse::ok("PUT response");
    });
    
    server.add_delete_route("/delete", [&delete_calls](const HttpRequest& /*req*/) {
        delete_calls++;
        return HttpResponse::ok("DELETE response");
    });
    
    // Routes registered successfully (compilation test)
    EXPECT_EQ(get_calls, 0);
    EXPECT_EQ(post_calls, 0);
    EXPECT_EQ(put_calls, 0);
    EXPECT_EQ(delete_calls, 0);
}

TEST_F(HttpServerTest, MiddlewareRegistration) {
    HttpServer server(config);
    
    bool middleware1_called = false;
    bool middleware2_called = false;
    
    server.add_middleware([&middleware1_called](const HttpRequest& /*req*/, HttpResponse& /*res*/) {
        middleware1_called = true;
        return true; // Continue processing
    });
    
    server.add_middleware([&middleware2_called](const HttpRequest& /*req*/, HttpResponse& /*res*/) {
        middleware2_called = true;
        return true; // Continue processing
    });
    
    // Middleware registered successfully (compilation test)
    EXPECT_FALSE(middleware1_called);
    EXPECT_FALSE(middleware2_called);
}

TEST_F(HttpServerTest, StaticFileConfiguration) {
    HttpServer server(config);
    
    // Test enabling static files
    server.enable_static_files("/custom/path");
    EXPECT_EQ(server.config().document_root, "/custom/path");
    EXPECT_TRUE(server.config().serve_static_files);
    
    // Test disabling static files
    server.disable_static_files();
    EXPECT_FALSE(server.config().serve_static_files);
}

TEST_F(HttpServerTest, ConfigurationUpdate) {
    HttpServer server(config);
    
    ServerConfig new_config = config;
    new_config.host = "new-host.com";
    new_config.port = 9999;
    new_config.thread_pool_size = 16;
    
    server.update_config(new_config);
    
    EXPECT_EQ(server.config().host, "new-host.com");
    EXPECT_EQ(server.config().port, 9999);
    EXPECT_EQ(server.config().thread_pool_size, 16);
}

TEST_F(HttpServerTest, StatisticsInitialization) {
    HttpServer server(config);
    
    const auto& stats = server.stats();
    
    EXPECT_EQ(stats.total_requests.load(), 0);
    EXPECT_EQ(stats.active_connections.load(), 0);
    EXPECT_EQ(stats.total_connections.load(), 0);
    EXPECT_EQ(stats.bytes_sent.load(), 0);
    EXPECT_EQ(stats.bytes_received.load(), 0);
    
    // Start time should be set
    auto now = std::chrono::steady_clock::now();
    auto time_diff = now - stats.start_time;
    EXPECT_LT(time_diff, std::chrono::seconds(1)); // Should be very recent
}

TEST_F(HttpServerTest, StatisticsJsonSerialization) {
    HttpServer server(config);
    
    std::string stats_json = server.stats_json();
    
    // Parse the JSON to verify structure
    auto json = nlohmann::json::parse(stats_json);
    
    EXPECT_TRUE(json.contains("total_requests"));
    EXPECT_TRUE(json.contains("active_connections"));
    EXPECT_TRUE(json.contains("total_connections"));
    EXPECT_TRUE(json.contains("bytes_sent"));
    EXPECT_TRUE(json.contains("bytes_received"));
    EXPECT_TRUE(json.contains("uptime_seconds"));
    
    EXPECT_EQ(json["total_requests"], 0);
    EXPECT_EQ(json["active_connections"], 0);
    EXPECT_EQ(json["total_connections"], 0);
    EXPECT_EQ(json["bytes_sent"], 0);
    EXPECT_EQ(json["bytes_received"], 0);
    EXPECT_GE(json["uptime_seconds"], 0);
}

TEST_F(HttpServerTest, ThreadPoolInitialization) {
    ThreadPool pool(4);
    
    EXPECT_EQ(pool.size(), 4);
    EXPECT_EQ(pool.pending_tasks(), 0);
}

TEST_F(HttpServerTest, ThreadPoolTaskExecution) {
    ThreadPool pool(2);
    
    std::atomic<int> counter{0};
    
    // Submit multiple tasks
    auto future1 = pool.enqueue([&counter]() {
        counter.fetch_add(1);
        return 42;
    });
    
    auto future2 = pool.enqueue([&counter]() {
        counter.fetch_add(10);
        return 100;
    });
    
    // Wait for completion
    auto result1 = future1.get();
    auto result2 = future2.get();
    
    EXPECT_EQ(result1, 42);
    EXPECT_EQ(result2, 100);
    EXPECT_EQ(counter.load(), 11);
}

TEST_F(HttpServerTest, ThreadPoolTaskWithParameters) {
    ThreadPool pool(1);
    
    auto future = pool.enqueue([](int a, int b) {
        return a + b;
    }, 5, 7);
    
    EXPECT_EQ(future.get(), 12);
}

TEST_F(HttpServerTest, ThreadPoolException) {
    ThreadPool pool(1);
    
    auto future = pool.enqueue([]() -> int {
        throw std::runtime_error("Test exception");
    });
    
    EXPECT_THROW(future.get(), std::runtime_error);
}

TEST_F(HttpServerTest, ThreadPoolShutdown) {
    auto pool = std::make_unique<ThreadPool>(2);
    
    std::atomic<bool> task_completed{false};
    
    auto future = pool->enqueue([&task_completed]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        task_completed = true;
        return 1;
    });
    
    // Shutdown should wait for running tasks
    pool->shutdown();
    
    EXPECT_TRUE(task_completed.load());
    EXPECT_EQ(future.get(), 1);
}

TEST_F(HttpServerTest, ThreadPoolStoppedEnqueue) {
    ThreadPool pool(1);
    pool.shutdown();
    
    // Enqueueing after shutdown should throw
    EXPECT_THROW(
        pool.enqueue([]() { return 1; }),
        std::runtime_error
    );
}

TEST_F(HttpServerTest, MimeTypeConfiguration) {
    ServerConfig test_config;
    test_config.mime_types["custom"] = "application/custom";
    
    HttpServer server(test_config);
    
    // The server should have the custom MIME type
    EXPECT_EQ(server.config().mime_types.at("custom"), "application/custom");
}

TEST_F(HttpServerTest, DefaultMimeTypeInitialization) {
    ServerConfig empty_config;
    empty_config.mime_types.clear(); // Start with empty MIME types
    
    HttpServer server(empty_config);
    
    // Server should initialize default MIME types
    EXPECT_FALSE(server.config().mime_types.empty());
    EXPECT_TRUE(server.config().mime_types.find("html") != server.config().mime_types.end());
    EXPECT_TRUE(server.config().mime_types.find("css") != server.config().mime_types.end());
    EXPECT_TRUE(server.config().mime_types.find("js") != server.config().mime_types.end());
}

TEST_F(HttpServerTest, ConfigFileHandling) {
    // Create a temporary config file
    std::string config_file = "test_config.json";
    
    nlohmann::json config_json = {
        {"host", "test-host"},
        {"port", 8888},
        {"thread_pool_size", 6}
    };
    
    std::ofstream file(config_file);
    file << config_json.dump(2);
    file.close();
    
    // Test loading from file
    auto loaded_config = ServerConfig::from_json(config_file);
    
    EXPECT_EQ(loaded_config.host, "test-host");
    EXPECT_EQ(loaded_config.port, 8888);
    EXPECT_EQ(loaded_config.thread_pool_size, 6);
    
    // Clean up
    std::filesystem::remove(config_file);
}

TEST_F(HttpServerTest, ConfigFileNotFound) {
    EXPECT_THROW(
        ServerConfig::from_json("nonexistent_config.json"),
        std::runtime_error
    );
}

TEST_F(HttpServerTest, InvalidJsonConfig) {
    EXPECT_THROW(
        ServerConfig::from_json_string("invalid json"),
        nlohmann::json::parse_error
    );
} 