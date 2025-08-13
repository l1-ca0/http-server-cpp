#include "rate_limiter.hpp"
#include <algorithm>
#include <sstream>
#include <thread>
#include <cassert>

namespace http_server {

// TokenBucketLimiter implementation
TokenBucketLimiter::TokenBucketLimiter(size_t capacity, size_t refill_rate, 
                                      std::chrono::seconds refill_interval)
    : capacity_(capacity), refill_rate_(refill_rate), refill_interval_(refill_interval) {}

RateLimitResult TokenBucketLimiter::check_rate_limit(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto now = std::chrono::steady_clock::now();
    
    // Find or create bucket
    auto it = buckets_.find(key);
    if (it == buckets_.end()) {
        it = buckets_.emplace(key, BucketState(capacity_)).first;
    }
    auto& bucket = it->second;
    
    // Calculate time elapsed since last refill
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - bucket.last_refill);
    
    // Refill tokens based on elapsed time
    if (elapsed >= refill_interval_) {
        size_t intervals = elapsed.count() / refill_interval_.count();
        size_t tokens_to_add = intervals * refill_rate_;
        bucket.tokens = std::min(capacity_, bucket.tokens + tokens_to_add);
        bucket.last_refill = now;
    }
    
    RateLimitResult result;
    if (bucket.tokens > 0) {
        bucket.tokens--;
        result.allowed = true;
        result.remaining = bucket.tokens;
        result.limit_type = "token_bucket";
    } else {
        result.allowed = false;
        result.remaining = 0;
        result.reset_time = std::chrono::seconds(
            (refill_interval_.count() - elapsed.count() % refill_interval_.count()));
        result.limit_type = "token_bucket";
        result.reason = "Token bucket exhausted";
    }
    
    return result;
}

void TokenBucketLimiter::cleanup_expired() {
    std::lock_guard<std::mutex> lock(mutex_);
    auto now = std::chrono::steady_clock::now();
    
    auto it = buckets_.begin();
    while (it != buckets_.end()) {
        auto elapsed = std::chrono::duration_cast<std::chrono::minutes>(now - it->second.last_refill);
        if (elapsed.count() > 60) {  // Remove inactive buckets after 1 hour
            it = buckets_.erase(it);
        } else {
            ++it;
        }
    }
}

// FixedWindowLimiter implementation
FixedWindowLimiter::FixedWindowLimiter(size_t max_requests, std::chrono::seconds window_duration)
    : max_requests_(max_requests), window_duration_(window_duration) {}

RateLimitResult FixedWindowLimiter::check_rate_limit(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto now = std::chrono::steady_clock::now();
    
    // Find or create window
    auto& window = windows_[key];
    
    // Check if we need to reset the window
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - window.window_start);
    if (elapsed >= window_duration_) {
        window.count = 0;
        window.window_start = now;
    }
    
    RateLimitResult result;
    if (window.count < max_requests_) {
        window.count++;
        result.allowed = true;
        result.remaining = max_requests_ - window.count;
        result.limit_type = "fixed_window";
    } else {
        result.allowed = false;
        result.remaining = 0;
        result.reset_time = std::chrono::seconds(
            window_duration_.count() - elapsed.count());
        result.limit_type = "fixed_window";
        result.reason = "Fixed window limit exceeded";
    }
    
    return result;
}

void FixedWindowLimiter::cleanup_expired() {
    std::lock_guard<std::mutex> lock(mutex_);
    auto now = std::chrono::steady_clock::now();
    
    auto it = windows_.begin();
    while (it != windows_.end()) {
        auto elapsed = std::chrono::duration_cast<std::chrono::minutes>(now - it->second.window_start);
        if (elapsed.count() > 60) {  // Remove inactive windows after 1 hour
            it = windows_.erase(it);
        } else {
            ++it;
        }
    }
}

// SlidingWindowLimiter implementation
SlidingWindowLimiter::SlidingWindowLimiter(size_t max_requests, std::chrono::seconds window_duration)
    : max_requests_(max_requests), window_duration_(window_duration) {}

RateLimitResult SlidingWindowLimiter::check_rate_limit(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto now = std::chrono::steady_clock::now();
    auto& requests = request_logs_[key];
    
    // Remove expired requests
    auto cutoff = now - window_duration_;
    requests.erase(
        std::remove_if(requests.begin(), requests.end(),
                      [cutoff](const RequestRecord& record) {
                          return record.timestamp < cutoff;
                      }),
        requests.end());
    
    RateLimitResult result;
    if (requests.size() < max_requests_) {
        requests.emplace_back();
        result.allowed = true;
        result.remaining = max_requests_ - requests.size();
        result.limit_type = "sliding_window";
    } else {
        result.allowed = false;
        result.remaining = 0;
        // Find the oldest request to determine reset time
        if (!requests.empty()) {
            auto oldest = *std::min_element(requests.begin(), requests.end(),
                [](const RequestRecord& a, const RequestRecord& b) {
                    return a.timestamp < b.timestamp;
                });
            auto reset_at = oldest.timestamp + window_duration_;
            result.reset_time = std::chrono::duration_cast<std::chrono::seconds>(reset_at - now);
        }
        result.limit_type = "sliding_window";
        result.reason = "Sliding window limit exceeded";
    }
    
    return result;
}

void SlidingWindowLimiter::cleanup_expired() {
    std::lock_guard<std::mutex> lock(mutex_);
    auto now = std::chrono::steady_clock::now();
    auto cutoff = now - std::chrono::hours(1);  // Remove logs older than 1 hour
    
    auto it = request_logs_.begin();
    while (it != request_logs_.end()) {
        auto& requests = it->second;
        requests.erase(
            std::remove_if(requests.begin(), requests.end(),
                          [cutoff](const RequestRecord& record) {
                              return record.timestamp < cutoff;
                          }),
            requests.end());
        
        if (requests.empty()) {
            it = request_logs_.erase(it);
        } else {
            ++it;
        }
    }
}

// RateLimiter implementation
RateLimiter::RateLimiter(const RateLimitConfig& config) : config_(config) {
    // Create algorithm based on strategy
    switch (config_.strategy) {
        case RateLimitStrategy::TOKEN_BUCKET:
            algorithm_ = std::make_unique<TokenBucketLimiter>(
                config_.burst_capacity, config_.max_requests, config_.window_duration);
            break;
        case RateLimitStrategy::FIXED_WINDOW:
            algorithm_ = std::make_unique<FixedWindowLimiter>(
                config_.max_requests, config_.window_duration);
            break;
        case RateLimitStrategy::SLIDING_WINDOW:
            algorithm_ = std::make_unique<SlidingWindowLimiter>(
                config_.max_requests, config_.window_duration);
            break;
        default:
            algorithm_ = std::make_unique<TokenBucketLimiter>(
                config_.burst_capacity, config_.max_requests, config_.window_duration);
    }
    
    // Start cleanup thread
    cleanup_thread_ = std::thread(&RateLimiter::cleanup_worker, this);
}

RateLimiter::~RateLimiter() {
    stop_cleanup_ = true;
    cleanup_cv_.notify_one();
    if (cleanup_thread_.joinable()) {
        cleanup_thread_.join();
    }
}

void RateLimiter::cleanup_worker() {
    while (!stop_cleanup_) {
        std::unique_lock<std::mutex> lock(cleanup_mutex_);
        cleanup_cv_.wait_for(lock, std::chrono::minutes(5), [this] { return stop_cleanup_.load(); });
        if (!stop_cleanup_) {
            algorithm_->cleanup_expired();
        }
    }
}

std::string RateLimiter::extract_key(const HttpRequest& request) const {
    if (config_.key_extractor) {
        return config_.key_extractor(request);
    }
    return RateLimitKeyExtractors::ip_address(request);
}

RateLimitResult RateLimiter::check_request(const HttpRequest& request) {
    if (!config_.enabled) {
        return RateLimitResult{true, SIZE_MAX, std::chrono::seconds(0), "disabled", ""};
    }
    
    std::string key = extract_key(request);
    return algorithm_->check_rate_limit(key);
}

void RateLimiter::update_config(const RateLimitConfig& config) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    config_ = config;
    
    // Recreate algorithm if strategy changed
    switch (config_.strategy) {
        case RateLimitStrategy::TOKEN_BUCKET:
            algorithm_ = std::make_unique<TokenBucketLimiter>(
                config_.burst_capacity, config_.max_requests, config_.window_duration);
            break;
        case RateLimitStrategy::FIXED_WINDOW:
            algorithm_ = std::make_unique<FixedWindowLimiter>(
                config_.max_requests, config_.window_duration);
            break;
        case RateLimitStrategy::SLIDING_WINDOW:
            algorithm_ = std::make_unique<SlidingWindowLimiter>(
                config_.max_requests, config_.window_duration);
            break;
        case RateLimitStrategy::LEAKY_BUCKET:
            // Leaky bucket implementation not yet available
            algorithm_ = std::make_unique<TokenBucketLimiter>(
                config_.burst_capacity, config_.max_requests, config_.window_duration);
            break;
    }
}

RateLimitConfig RateLimiter::get_config() const {
    std::lock_guard<std::mutex> lock(config_mutex_);
    return config_;
}

std::function<bool(const HttpRequest&, HttpResponse&)> RateLimiter::create_middleware() {
    return [this](const HttpRequest& request, HttpResponse& response) -> bool {
        auto result = this->check_request(request);
        
        if (!result.allowed) {
            // Set rate limit headers
            response.set_header("X-RateLimit-Limit", std::to_string(config_.max_requests));
            response.set_header("X-RateLimit-Remaining", std::to_string(result.remaining));
            response.set_header("X-RateLimit-Reset", std::to_string(result.reset_time.count()));
            response.set_header("X-RateLimit-Type", result.limit_type);
            
            if (config_.rate_limit_response) {
                response = config_.rate_limit_response();
            } else {
                response.set_status(HttpStatus::TOO_MANY_REQUESTS)
                       .set_json(R"({"error": "Rate limit exceeded", "reason": ")" + result.reason + "\"}");
            }
            return false;  // Stop processing
        }
        
        // Add rate limit info headers for successful requests
        response.set_header("X-RateLimit-Limit", std::to_string(config_.max_requests));
        response.set_header("X-RateLimit-Remaining", std::to_string(result.remaining));
        
        return true;  // Continue processing
    };
}

// Key extractors implementation
std::string RateLimitKeyExtractors::ip_address(const HttpRequest& request) {
    // Try X-Forwarded-For first (for reverse proxy scenarios)
    auto forwarded = request.get_header("X-Forwarded-For");
    if (forwarded) {
        std::string forwarded_str = *forwarded;
        size_t comma_pos = forwarded_str.find(',');
        if (comma_pos != std::string::npos) {
            return forwarded_str.substr(0, comma_pos);
        }
        return forwarded_str;
    }
    
    // Try X-Real-IP
    auto real_ip = request.get_header("X-Real-IP");
    if (real_ip) {
        return *real_ip;
    }
    
    // Fallback to connection remote address (would need server connection info)
    return "127.0.0.1";  // Placeholder - would need actual client IP from connection
}

std::string RateLimitKeyExtractors::user_id(const HttpRequest& request) {
    auto auth = request.get_header("Authorization");
    if (auth) {
        // Extract user ID from JWT token or API key
        // This is a simplified example
        std::string auth_str = *auth;
        if (auth_str.starts_with("Bearer ")) {
            return auth_str.substr(7);  // Remove "Bearer " prefix
        }
    }
    
    // Fallback to IP if no user ID available
    return ip_address(request);
}

std::string RateLimitKeyExtractors::api_key(const HttpRequest& request) {
    auto api_key = request.get_header("X-API-Key");
    if (api_key) {
        return *api_key;
    }
    
    // Try query parameter
    auto key_param = request.get_query_param("api_key");
    if (key_param) {
        return *key_param;
    }
    
    return ip_address(request);  // Fallback
}

std::string RateLimitKeyExtractors::ip_and_user_agent(const HttpRequest& request) {
    std::string ip = ip_address(request);
    auto user_agent = request.get_header("User-Agent");
    
    std::ostringstream key;
    key << ip << "|" << (user_agent ? *user_agent : "unknown");
    return key.str();
}

std::string RateLimitKeyExtractors::endpoint_path(const HttpRequest& request) {
    return request.path();
}

// Middleware factory implementations
std::function<bool(const HttpRequest&, HttpResponse&)> 
RateLimitMiddleware::create_global_limiter(const RateLimitConfig& config) {
    static auto limiter = std::make_shared<RateLimiter>(config);
    return limiter->create_middleware();
}

std::function<bool(const HttpRequest&, HttpResponse&)> 
RateLimitMiddleware::create_endpoint_limiter(const std::string& endpoint, const RateLimitConfig& config) {
    static std::unordered_map<std::string, std::shared_ptr<RateLimiter>> limiters;
    
    if (limiters.find(endpoint) == limiters.end()) {
        auto endpoint_config = config;
        endpoint_config.key_extractor = [endpoint](const HttpRequest& request) {
            return RateLimitKeyExtractors::ip_address(request) + "|" + endpoint;
        };
        limiters[endpoint] = std::make_shared<RateLimiter>(endpoint_config);
    }
    
    return limiters[endpoint]->create_middleware();
}

std::function<bool(const HttpRequest&, HttpResponse&)> 
RateLimitMiddleware::create_user_limiter(const RateLimitConfig& config) {
    static auto limiter = std::make_shared<RateLimiter>(config);
    return limiter->create_middleware();
}

} // namespace http_server
