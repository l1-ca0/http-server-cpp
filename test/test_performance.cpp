#include <gtest/gtest.h>
#include "server.hpp"
#include "request.hpp"
#include "response.hpp"
#include "thread_pool.hpp"
#include <chrono>
#include <thread>
#include <atomic>
#include <future>
#include <random>
#include <vector>

using namespace http_server;

class PerformanceTest : public ::testing::Test {
protected:
    void SetUp() override {
        config.host = "127.0.0.1";
        config.port = 0;
        config.thread_pool_size = 4;
        config.document_root = "./test_perf";
        config.enable_logging = false;
        config.serve_static_files = true;
        
        std::filesystem::create_directories(config.document_root);
    }
    
    void TearDown() override {
        std::filesystem::remove_all(config.document_root);
    }
    
    ServerConfig config;
};

// Concurrent Request Parsing
TEST_F(PerformanceTest, ConcurrentRequestParsing) {
    const int num_threads = 10;
    const int requests_per_thread = 100;
    std::atomic<int> successful_parses{0};
    std::atomic<int> failed_parses{0};
    
    std::vector<std::future<void>> futures;
    
    for (int t = 0; t < num_threads; ++t) {
        futures.push_back(std::async(std::launch::async, [&, t]() {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> path_dist(1, 1000);
            
            for (int i = 0; i < requests_per_thread; ++i) {
                std::string path = "/path" + std::to_string(path_dist(gen));
                std::string raw_request = 
                    "GET " + path + " HTTP/1.1\r\n"
                    "Host: localhost\r\n"
                    "User-Agent: LoadTest-" + std::to_string(t) + "-" + std::to_string(i) + "\r\n"
                    "\r\n";
                
                auto request = HttpRequest::parse(raw_request);
                if (request.has_value()) {
                    successful_parses.fetch_add(1);
                } else {
                    failed_parses.fetch_add(1);
                }
            }
        }));
    }
    
    // Wait for all threads to complete
    for (auto& future : futures) {
        future.wait();
    }
    
    EXPECT_EQ(successful_parses.load(), num_threads * requests_per_thread);
    EXPECT_EQ(failed_parses.load(), 0);
}

// Memory Usage Under Load
TEST_F(PerformanceTest, MemoryUsageUnderLoad) {
    std::vector<HttpRequest> requests;
    requests.reserve(1000);
    
    // Create 1000 requests to test memory usage
    for (int i = 0; i < 1000; ++i) {
        std::string body = "Request body " + std::to_string(i) + " with some content";
        std::string raw_request = 
            "POST /api/test" + std::to_string(i) + " HTTP/1.1\r\n"
            "Host: localhost\r\n"
            "Content-Type: application/json\r\n"
            "Content-Length: " + std::to_string(body.length()) + "\r\n"
            "\r\n" + body;
        
        auto request = HttpRequest::parse(raw_request);
        if (request.has_value()) {
            requests.push_back(std::move(*request));
        }
    }
    
    EXPECT_EQ(requests.size(), 1000);
    
    // Verify all requests are valid
    for (const auto& req : requests) {
        EXPECT_TRUE(req.is_valid());
        EXPECT_EQ(req.method(), HttpMethod::POST);
    }
}

// Thread Pool Stress Test
TEST_F(PerformanceTest, ThreadPoolStressTest) {
    ThreadPool pool(8);
    const int num_tasks = 1000;
    std::atomic<int> completed_tasks{0};
    std::vector<std::future<int>> futures;
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Submit many tasks quickly
    for (int i = 0; i < num_tasks; ++i) {
        futures.push_back(pool.enqueue([&completed_tasks, i]() {
            // Simulate some work
            std::this_thread::sleep_for(std::chrono::microseconds(100));
            completed_tasks.fetch_add(1);
            return i * 2;
        }));
    }
    
    // Wait for all tasks to complete
    for (int i = 0; i < num_tasks; ++i) {
        int result = futures[i].get();
        EXPECT_EQ(result, i * 2);
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    EXPECT_EQ(completed_tasks.load(), num_tasks);
    EXPECT_LT(duration.count(), 10000); // Should complete within 10 seconds
}

// Large Response Generation
TEST_F(PerformanceTest, LargeResponseGeneration) {
    const size_t response_sizes[] = {1024, 1024*1024, 10*1024*1024}; // 1KB, 1MB, 10MB
    
    for (size_t size : response_sizes) {
        auto start_time = std::chrono::high_resolution_clock::now();
        
        HttpResponse response;
        std::string large_body(size, 'X');
        response.set_body(large_body);
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        
        EXPECT_EQ(response.body().size(), size);
        EXPECT_EQ(response.get_header("Content-Length"), std::to_string(size));
        
        // Performance check - should be reasonably fast
        EXPECT_LT(duration.count(), 100000); // Less than 100ms
    }
}

// Concurrent Statistics Updates
TEST_F(PerformanceTest, ConcurrentStatisticsUpdates) {
    HttpServer server(config);
    const int num_threads = 20;
    const int updates_per_thread = 1000;
    
    std::vector<std::future<void>> futures;
    
    for (int t = 0; t < num_threads; ++t) {
        futures.push_back(std::async(std::launch::async, [&]() {
            for (int i = 0; i < updates_per_thread; ++i) {
                // Simulate statistics updates that would happen during request processing
                auto& stats = const_cast<HttpServer::Statistics&>(server.stats());
                stats.total_requests.fetch_add(1);
                stats.bytes_sent.fetch_add(1024);
                stats.bytes_received.fetch_add(512);
                stats.active_connections.fetch_add(1);
                stats.active_connections.fetch_sub(1);
            }
        }));
    }
    
    // Wait for all threads
    for (auto& future : futures) {
        future.wait();
    }
    
    const auto& stats = server.stats();
    EXPECT_EQ(stats.total_requests.load(), num_threads * updates_per_thread);
    EXPECT_EQ(stats.bytes_sent.load(), num_threads * updates_per_thread * 1024);
    EXPECT_EQ(stats.bytes_received.load(), num_threads * updates_per_thread * 512);
    EXPECT_EQ(stats.active_connections.load(), 0); // Should balance out
}

// Rapid Configuration Updates
TEST_F(PerformanceTest, RapidConfigurationUpdates) {
    HttpServer server(config);
    
    for (int i = 0; i < 100; ++i) {
        ServerConfig new_config = config;
        new_config.port = 8000 + i;
        new_config.thread_pool_size = 2 + (i % 8);
        new_config.max_connections = 1000 + i * 10;
        
        server.update_config(new_config);
        
        EXPECT_EQ(server.config().port, 8000 + i);
        EXPECT_EQ(server.config().thread_pool_size, 2 + (i % 8));
        EXPECT_EQ(server.config().max_connections, 1000 + i * 10);
    }
}

// Edge Case: Malformed HTTP Parsing
TEST_F(PerformanceTest, MalformedHttpParsing) {
    std::vector<std::string> malformed_requests = {
        "GET",
        "GET /",
        "GET / HTTP",
        "GET / HTTP/",
        "GET / HTTP/1.1",
        "GET / HTTP/1.1\r",
        "GET / HTTP/1.1\n",
        "\r\n\r\n",
        "GET / HTTP/1.1\r\nHost:",
        "GET / HTTP/1.1\r\nHost: \r\n",
        "INVALID METHOD / HTTP/1.1\r\nHost: test\r\n\r\n",
        "GET / HTTP/999.999\r\nHost: test\r\n\r\n",
        "",
        "HTTP/1.1 200 OK\r\n\r\n", // Response instead of request
        std::string(1000000, 'A'), // 1MB of 'A' characters
    };
    
    int parsed_count = 0;
    int failed_count = 0;
    
    for (const auto& malformed : malformed_requests) {
        auto request = HttpRequest::parse(malformed);
        if (request.has_value()) {
            parsed_count++;
        } else {
            failed_count++;
        }
    }
    
    // Most should fail to parse, but parsing should not crash
    EXPECT_GT(failed_count, 0);
    EXPECT_LT(parsed_count, malformed_requests.size());
}

// Binary Data Handling
TEST_F(PerformanceTest, BinaryDataHandling) {
    std::vector<uint8_t> binary_data;
    binary_data.reserve(10000);
    
    // Generate random binary data including null bytes
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> byte_dist(0, 255);
    
    for (int i = 0; i < 10000; ++i) {
        binary_data.push_back(static_cast<uint8_t>(byte_dist(gen)));
    }
    
    std::string binary_string(binary_data.begin(), binary_data.end());
    
    std::string raw_request = 
        "POST /upload HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Type: application/octet-stream\r\n"
        "Content-Length: " + std::to_string(binary_string.length()) + "\r\n"
        "\r\n" + binary_string;
    
    auto request = HttpRequest::parse(raw_request);
    
    if (request.has_value()) {
        EXPECT_EQ(request->body().length(), binary_data.size());
        EXPECT_EQ(request->content_type(), "application/octet-stream");
    }
    
    // Test response with binary data
    HttpResponse response;
    response.set_body(binary_string);
    response.set_header("Content-Type", "application/octet-stream");
    
    EXPECT_EQ(response.body().length(), binary_data.size());
}

// Timeout Simulation
TEST_F(PerformanceTest, TimeoutSimulation) {
    ThreadPool pool(4);
    
    // Submit a task that takes longer than typical timeout
    auto long_task = pool.enqueue([]() {
        std::this_thread::sleep_for(std::chrono::seconds(2));
        return 42;
    });
    
    // Submit normal tasks alongside
    auto quick_task1 = pool.enqueue([]() { return 1; });
    auto quick_task2 = pool.enqueue([]() { return 2; });
    
    // Quick tasks should complete first
    EXPECT_EQ(quick_task1.get(), 1);
    EXPECT_EQ(quick_task2.get(), 2);
    
    // Long task should eventually complete
    auto start = std::chrono::steady_clock::now();
    int result = long_task.get();
    auto duration = std::chrono::steady_clock::now() - start;
    
    EXPECT_EQ(result, 42);
    EXPECT_GE(std::chrono::duration_cast<std::chrono::seconds>(duration).count(), 2);
}

// Memory Fragmentation Test
TEST_F(PerformanceTest, MemoryFragmentation) {
    std::vector<HttpResponse> responses;
    responses.reserve(1000);
    
    // Create responses of varying sizes to test memory fragmentation
    for (int i = 0; i < 1000; ++i) {
        HttpResponse response;
        
        // Vary response sizes: small, medium, large
        size_t body_size = (i % 3 == 0) ? 100 : (i % 3 == 1) ? 10000 : 1000000;
        std::string body(body_size, 'X');
        
        response.set_body(body);
        responses.push_back(std::move(response));
    }
    
    // Verify all responses are correct
    for (size_t i = 0; i < responses.size(); ++i) {
        size_t expected_size = (i % 3 == 0) ? 100 : (i % 3 == 1) ? 10000 : 1000000;
        EXPECT_EQ(responses[i].body().size(), expected_size);
    }
} 