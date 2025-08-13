#include <gtest/gtest.h>
#include "rate_limiter.hpp"
#include "request.hpp"
#include <thread>
#include <chrono>

using namespace http_server;

class RateLimiterTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Default configuration for testing
        config_.max_requests = 5;
        config_.window_duration = std::chrono::seconds(1);
        config_.burst_capacity = 3;
        config_.strategy = RateLimitStrategy::TOKEN_BUCKET;
        config_.enabled = true;
    }
    
    HttpRequest create_test_request(const std::string& client_ip = "192.168.1.1") {
        HttpRequest request;
        request.set_header("X-Real-IP", client_ip);
        request.set_path("/api/test");
        request.set_method(HttpMethod::GET);
        return request;
    }
    
    RateLimitConfig config_;
};

// Token Bucket Tests
TEST_F(RateLimiterTest, TokenBucketAllowsBurstRequests) {
    config_.strategy = RateLimitStrategy::TOKEN_BUCKET;
    RateLimiter limiter(config_);
    
    auto request = create_test_request();
    
    // Should allow burst_capacity requests immediately
    for (int i = 0; i < static_cast<int>(config_.burst_capacity); ++i) {
        auto result = limiter.check_request(request);
        EXPECT_TRUE(result.allowed);
        EXPECT_EQ(result.limit_type, "token_bucket");
    }
    
    // Next request should be denied
    auto result = limiter.check_request(request);
    EXPECT_FALSE(result.allowed);
    EXPECT_EQ(result.reason, "Token bucket exhausted");
}

TEST_F(RateLimiterTest, TokenBucketRefillsOverTime) {
    config_.strategy = RateLimitStrategy::TOKEN_BUCKET;
    config_.window_duration = std::chrono::seconds(1);
    config_.max_requests = 2;  // 2 tokens per second
    config_.burst_capacity = 1;
    
    RateLimiter limiter(config_);
    auto request = create_test_request();
    
    // Use up the initial token
    auto result = limiter.check_request(request);
    EXPECT_TRUE(result.allowed);
    
    // Should be denied immediately
    result = limiter.check_request(request);
    EXPECT_FALSE(result.allowed);
    
    // Wait for refill
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    
    // Should be allowed again after refill
    result = limiter.check_request(request);
    EXPECT_TRUE(result.allowed);
}

// Fixed Window Tests
TEST_F(RateLimiterTest, FixedWindowEnforcesLimit) {
    config_.strategy = RateLimitStrategy::FIXED_WINDOW;
    config_.max_requests = 3;
    config_.window_duration = std::chrono::seconds(2);
    
    RateLimiter limiter(config_);
    auto request = create_test_request();
    
    // Should allow max_requests within window
    for (int i = 0; i < static_cast<int>(config_.max_requests); ++i) {
        auto result = limiter.check_request(request);
        EXPECT_TRUE(result.allowed);
        EXPECT_EQ(result.remaining, config_.max_requests - i - 1);
    }
    
    // Next request should be denied
    auto result = limiter.check_request(request);
    EXPECT_FALSE(result.allowed);
    EXPECT_EQ(result.reason, "Fixed window limit exceeded");
}

TEST_F(RateLimiterTest, FixedWindowResetsAfterDuration) {
    config_.strategy = RateLimitStrategy::FIXED_WINDOW;
    config_.max_requests = 2;
    config_.window_duration = std::chrono::seconds(1);
    
    RateLimiter limiter(config_);
    auto request = create_test_request();
    
    // Use up the window
    for (int i = 0; i < static_cast<int>(config_.max_requests); ++i) {
        auto result = limiter.check_request(request);
        EXPECT_TRUE(result.allowed);
    }
    
    // Should be denied
    auto result = limiter.check_request(request);
    EXPECT_FALSE(result.allowed);
    
    // Wait for window reset
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    
    // Should be allowed again in new window
    result = limiter.check_request(request);
    EXPECT_TRUE(result.allowed);
}

// Sliding Window Tests
TEST_F(RateLimiterTest, SlidingWindowEnforcesLimit) {
    config_.strategy = RateLimitStrategy::SLIDING_WINDOW;
    config_.max_requests = 3;
    config_.window_duration = std::chrono::seconds(2);
    
    RateLimiter limiter(config_);
    auto request = create_test_request();
    
    // Should allow max_requests
    for (int i = 0; i < static_cast<int>(config_.max_requests); ++i) {
        auto result = limiter.check_request(request);
        EXPECT_TRUE(result.allowed);
    }
    
    // Next request should be denied
    auto result = limiter.check_request(request);
    EXPECT_FALSE(result.allowed);
    EXPECT_EQ(result.reason, "Sliding window limit exceeded");
}

// Multi-client Tests
TEST_F(RateLimiterTest, DifferentClientsHaveSeparateLimits) {
    config_.strategy = RateLimitStrategy::TOKEN_BUCKET;
    config_.burst_capacity = 2;
    
    RateLimiter limiter(config_);
    
    auto request1 = create_test_request("192.168.1.1");
    auto request2 = create_test_request("192.168.1.2");
    
    // Both clients should be able to use their full burst capacity
    for (int i = 0; i < static_cast<int>(config_.burst_capacity); ++i) {
        auto result1 = limiter.check_request(request1);
        auto result2 = limiter.check_request(request2);
        
        EXPECT_TRUE(result1.allowed);
        EXPECT_TRUE(result2.allowed);
    }
    
    // Both should be denied after burst
    auto result1 = limiter.check_request(request1);
    auto result2 = limiter.check_request(request2);
    
    EXPECT_FALSE(result1.allowed);
    EXPECT_FALSE(result2.allowed);
}

// Configuration Tests
TEST_F(RateLimiterTest, DisabledLimiterAllowsAllRequests) {
    config_.enabled = false;
    RateLimiter limiter(config_);
    
    auto request = create_test_request();
    
    // Should allow many requests when disabled
    for (int i = 0; i < 100; ++i) {
        auto result = limiter.check_request(request);
        EXPECT_TRUE(result.allowed);
        EXPECT_EQ(result.limit_type, "disabled");
    }
}

TEST_F(RateLimiterTest, ConfigurationUpdate) {
    RateLimiter limiter(config_);
    auto request = create_test_request();
    
    // Use up initial capacity
    for (int i = 0; i < static_cast<int>(config_.burst_capacity); ++i) {
        auto result = limiter.check_request(request);
        EXPECT_TRUE(result.allowed);
    }
    
    // Should be denied
    auto result = limiter.check_request(request);
    EXPECT_FALSE(result.allowed);
    
    // Update configuration to increase capacity
    config_.burst_capacity = 10;
    limiter.update_config(config_);
    
    // Should be allowed again with new configuration
    result = limiter.check_request(request);
    EXPECT_TRUE(result.allowed);
}

// Key Extractor Tests
TEST_F(RateLimiterTest, CustomKeyExtractor) {
    config_.key_extractor = [](const HttpRequest& request) {
        auto user_id = request.get_header("User-ID");
        return user_id ? *user_id : "anonymous";
    };
    
    RateLimiter limiter(config_);
    
    // Create requests with different user IDs
    auto request1 = create_test_request();
    request1.set_header("User-ID", "user123");
    
    auto request2 = create_test_request();
    request2.set_header("User-ID", "user456");
    
    // Both users should have separate limits
    for (int i = 0; i < static_cast<int>(config_.burst_capacity); ++i) {
        auto result1 = limiter.check_request(request1);
        auto result2 = limiter.check_request(request2);
        
        EXPECT_TRUE(result1.allowed);
        EXPECT_TRUE(result2.allowed);
    }
}

// Middleware Tests
TEST_F(RateLimiterTest, MiddlewareIntegration) {
    RateLimiter limiter(config_);
    auto middleware = limiter.create_middleware();
    
    auto request = create_test_request();
    HttpResponse response;
    
    // Should allow requests within limit
    for (int i = 0; i < static_cast<int>(config_.burst_capacity); ++i) {
        bool continue_processing = middleware(request, response);
        EXPECT_TRUE(continue_processing);
        
        // Check rate limit headers are set
        EXPECT_TRUE(response.has_header("X-RateLimit-Limit"));
        auto limit_header = response.get_header("X-RateLimit-Limit");
    }
    
    // Should block when limit exceeded
    bool continue_processing = middleware(request, response);
    EXPECT_FALSE(continue_processing);
    EXPECT_EQ(response.status(), HttpStatus::TOO_MANY_REQUESTS);
    
    // Check rate limit headers
    EXPECT_TRUE(response.has_header("X-RateLimit-Remaining"));
    auto remaining_header = response.get_header("X-RateLimit-Remaining");
    EXPECT_EQ(remaining_header, "0");
}

// Performance Tests
TEST_F(RateLimiterTest, ConcurrentAccess) {
    config_.strategy = RateLimitStrategy::TOKEN_BUCKET;
    config_.burst_capacity = 1000;
    config_.max_requests = 1000;
    
    RateLimiter limiter(config_);
    
    const int num_threads = 10;
    const int requests_per_thread = 50;
    std::atomic<int> allowed_count{0};
    std::atomic<int> denied_count{0};
    
    std::vector<std::thread> threads;
    
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < requests_per_thread; ++i) {
                auto request = create_test_request("192.168.1." + std::to_string(t + 1));
                auto result = limiter.check_request(request);
                
                if (result.allowed) {
                    allowed_count++;
                } else {
                    denied_count++;
                }
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Most requests should be allowed given high capacity
    EXPECT_GT(allowed_count.load(), num_threads * requests_per_thread * 0.9);
}

// Key Extractor Function Tests
TEST_F(RateLimiterTest, KeyExtractors) {
    HttpRequest request;
    request.set_path("/api/users");
    request.set_header("X-Forwarded-For", "203.0.113.1, 192.168.1.1");
    request.set_header("X-API-Key", "test-api-key-123");
    request.set_header("User-Agent", "TestAgent/1.0");
    request.set_header("Authorization", "Bearer user-token-456");
    
    // Test IP extraction
    std::string ip = RateLimitKeyExtractors::ip_address(request);
    EXPECT_EQ(ip, "203.0.113.1");  // Should extract first IP from X-Forwarded-For
    
    // Test API key extraction
    std::string api_key = RateLimitKeyExtractors::api_key(request);
    EXPECT_EQ(api_key, "test-api-key-123");
    
    // Test user ID extraction
    std::string user_id = RateLimitKeyExtractors::user_id(request);
    EXPECT_EQ(user_id, "user-token-456");
    
    // Test endpoint path extraction
    std::string endpoint = RateLimitKeyExtractors::endpoint_path(request);
    EXPECT_EQ(endpoint, "/api/users");
    
    // Test combined IP and User-Agent
    std::string combined = RateLimitKeyExtractors::ip_and_user_agent(request);
    EXPECT_EQ(combined, "203.0.113.1|TestAgent/1.0");
}

// Cleanup Tests
TEST_F(RateLimiterTest, CleanupExpiredEntries) {
    config_.strategy = RateLimitStrategy::SLIDING_WINDOW;
    config_.window_duration = std::chrono::seconds(1);
    
    RateLimiter limiter(config_);
    
    // Create requests from multiple clients
    for (int i = 0; i < 10; ++i) {
        auto request = create_test_request("192.168.1." + std::to_string(i));
        limiter.check_request(request);
    }
    
    // Wait for entries to expire
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    
    // Trigger cleanup by making new requests
    for (int i = 0; i < 5; ++i) {
        auto request = create_test_request("10.0.0." + std::to_string(i));
        limiter.check_request(request);
    }
    
    // Test passes if no crashes occur during cleanup
    SUCCEED();
}
