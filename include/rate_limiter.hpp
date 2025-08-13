#pragma once

#include <memory>
#include <string>
#include <chrono>
#include <unordered_map>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <deque>
#include <functional>
#include "request.hpp"
#include "response.hpp"

namespace http_server {

/**
 * @brief Rate limiting result
 */
struct RateLimitResult {
    bool allowed = true;
    size_t remaining = 0;
    std::chrono::seconds reset_time{0};
    std::string limit_type;
    std::string reason;
};

/**
 * @brief Rate limiting strategies
 */
enum class RateLimitStrategy {
    TOKEN_BUCKET,
    FIXED_WINDOW,
    SLIDING_WINDOW,
    LEAKY_BUCKET
};

/**
 * @brief Rate limit configuration
 */
struct RateLimitConfig {
    size_t max_requests = 100;
    std::chrono::seconds window_duration{60};
    size_t burst_capacity = 10;
    RateLimitStrategy strategy = RateLimitStrategy::TOKEN_BUCKET;
    bool enabled = true;
    
    // Custom key extractor (default: IP address)
    std::function<std::string(const HttpRequest&)> key_extractor;
    
    // Custom response for rate limit exceeded
    std::function<HttpResponse()> rate_limit_response;
};

/**
 * @brief Abstract base class for rate limiting algorithms
 */
class RateLimitAlgorithm {
public:
    virtual ~RateLimitAlgorithm() = default;
    virtual RateLimitResult check_rate_limit(const std::string& key) = 0;
    virtual void cleanup_expired() = 0;
};

/**
 * @brief Token bucket rate limiter
 */
class TokenBucketLimiter : public RateLimitAlgorithm {
private:
    struct BucketState {
        size_t tokens;
        std::chrono::steady_clock::time_point last_refill;
        
        BucketState() : tokens(0), last_refill(std::chrono::steady_clock::now()) {}
        BucketState(size_t capacity) 
            : tokens(capacity), last_refill(std::chrono::steady_clock::now()) {}
    };
    
    size_t capacity_;
    size_t refill_rate_;
    std::chrono::seconds refill_interval_;
    mutable std::mutex mutex_;
    std::unordered_map<std::string, BucketState> buckets_;
    
public:
    TokenBucketLimiter(size_t capacity, size_t refill_rate, 
                      std::chrono::seconds refill_interval);
    
    RateLimitResult check_rate_limit(const std::string& key) override;
    void cleanup_expired() override;
};

/**
 * @brief Fixed window rate limiter
 */
class FixedWindowLimiter : public RateLimitAlgorithm {
private:
    struct WindowState {
        size_t count;
        std::chrono::steady_clock::time_point window_start;
        
        WindowState() : count(0), window_start(std::chrono::steady_clock::now()) {}
    };
    
    size_t max_requests_;
    std::chrono::seconds window_duration_;
    mutable std::mutex mutex_;
    std::unordered_map<std::string, WindowState> windows_;
    
public:
    FixedWindowLimiter(size_t max_requests, std::chrono::seconds window_duration);
    
    RateLimitResult check_rate_limit(const std::string& key) override;
    void cleanup_expired() override;
};

/**
 * @brief Sliding window rate limiter
 */
class SlidingWindowLimiter : public RateLimitAlgorithm {
private:
    struct RequestRecord {
        std::chrono::steady_clock::time_point timestamp;
        
        RequestRecord() : timestamp(std::chrono::steady_clock::now()) {}
    };
    
    size_t max_requests_;
    std::chrono::seconds window_duration_;
    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::vector<RequestRecord>> request_logs_;
    
public:
    SlidingWindowLimiter(size_t max_requests, std::chrono::seconds window_duration);
    
    RateLimitResult check_rate_limit(const std::string& key) override;
    void cleanup_expired() override;
};

/**
 * @brief Main rate limiter class
 */
class RateLimiter {
private:
    RateLimitConfig config_;
    std::unique_ptr<RateLimitAlgorithm> algorithm_;
    mutable std::mutex config_mutex_;
    
    // Cleanup thread
    std::thread cleanup_thread_;
    std::atomic<bool> stop_cleanup_{false};
    std::condition_variable cleanup_cv_;
    std::mutex cleanup_mutex_;
    
    void cleanup_worker();
    std::string extract_key(const HttpRequest& request) const;
    
public:
    explicit RateLimiter(const RateLimitConfig& config);
    ~RateLimiter();
    
    // Non-copyable
    RateLimiter(const RateLimiter&) = delete;
    RateLimiter& operator=(const RateLimiter&) = delete;
    
    // Check if request is allowed
    RateLimitResult check_request(const HttpRequest& request);
    
    // Configuration management
    void update_config(const RateLimitConfig& config);
    RateLimitConfig get_config() const;
    
    // Statistics
    size_t get_active_keys() const;
    void reset_all_limits();
    
    // Middleware integration
    std::function<bool(const HttpRequest&, HttpResponse&)> create_middleware();
};

/**
 * @brief Rate limiting middleware factory
 */
class RateLimitMiddleware {
public:
    // Create global rate limiter
    static std::function<bool(const HttpRequest&, HttpResponse&)> 
    create_global_limiter(const RateLimitConfig& config);
    
    // Create per-endpoint rate limiter
    static std::function<bool(const HttpRequest&, HttpResponse&)> 
    create_endpoint_limiter(const std::string& endpoint, const RateLimitConfig& config);
    
    // Create per-user rate limiter (requires authentication)
    static std::function<bool(const HttpRequest&, HttpResponse&)> 
    create_user_limiter(const RateLimitConfig& config);
    
    // Create adaptive rate limiter (adjusts based on server load)
    static std::function<bool(const HttpRequest&, HttpResponse&)> 
    create_adaptive_limiter(const RateLimitConfig& base_config);
};

/**
 * @brief Default key extractors
 */
class RateLimitKeyExtractors {
public:
    // Extract client IP address
    static std::string ip_address(const HttpRequest& request);
    
    // Extract user ID from Authorization header
    static std::string user_id(const HttpRequest& request);
    
    // Extract API key from header
    static std::string api_key(const HttpRequest& request);
    
    // Extract combination of IP and User-Agent
    static std::string ip_and_user_agent(const HttpRequest& request);
    
    // Extract endpoint path
    static std::string endpoint_path(const HttpRequest& request);
};

/**
 * @brief Rate limit statistics
 */
struct RateLimitStats {
    size_t total_requests = 0;
    size_t allowed_requests = 0;
    size_t blocked_requests = 0;
    size_t active_keys = 0;
    std::chrono::steady_clock::time_point start_time;
    
    double get_block_rate() const {
        return total_requests > 0 ? 
               static_cast<double>(blocked_requests) / total_requests : 0.0;
    }
    
    std::chrono::seconds get_uptime() const {
        return std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - start_time);
    }
};

} // namespace http_server
