#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <optional>
#include <string_view>

namespace http_server {

enum class HttpMethod {
    GET,
    POST,
    PUT,
    DELETE,
    HEAD,
    OPTIONS,
    PATCH,
    UNKNOWN
};

class HttpRequest {
public:
    HttpRequest() = default;
    ~HttpRequest() = default;
    
    static std::optional<HttpRequest> parse(std::string_view raw_request);
    
    HttpMethod method() const noexcept { return method_; }
    const std::string& path() const noexcept { return path_; }
    const std::string& version() const noexcept { return version_; }
    const std::string& body() const noexcept { return body_; }
    const std::unordered_map<std::string, std::string>& headers() const noexcept { return headers_; }
    const std::unordered_map<std::string, std::string>& query_params() const noexcept { return query_params_; }
    
    std::optional<std::string> get_header(const std::string& name) const;
    bool has_header(const std::string& name) const;
    size_t content_length() const;
    std::string content_type() const;
    
    std::optional<std::string> get_query_param(const std::string& name) const;
    bool has_query_param(const std::string& name) const;
    
    // Conditional request support
    std::optional<std::string> get_if_none_match() const;
    std::optional<std::string> get_if_modified_since() const;
    std::optional<std::string> get_if_match() const;
    std::optional<std::string> get_if_unmodified_since() const;
    bool is_conditional_request() const;
    
    bool is_valid() const noexcept { return is_valid_; }
    bool is_keep_alive() const;
    
    // Testing helper methods
    void set_header(const std::string& name, const std::string& value);
    void set_path(const std::string& path);
    void set_method(HttpMethod method);
    
    std::string to_string() const;
    static std::string method_to_string(HttpMethod method);
    static HttpMethod string_to_method(std::string_view method_str);
    
    bool is_valid_header_name(const std::string& name);
    bool is_valid_header_value(const std::string& value);
    static bool is_valid_http_version(const std::string& version);

private:
    HttpMethod method_{HttpMethod::UNKNOWN};
    std::string path_;
    std::string version_;
    std::unordered_map<std::string, std::string> headers_;
    std::unordered_map<std::string, std::string> query_params_;
    std::string body_;
    bool is_valid_{false};
    
    void parse_request_line(std::string_view line);
    void parse_header_line(std::string_view line);
    void parse_query_string(std::string_view query);
    void normalize_header_name(std::string& name) const;
    bool parse_chunked_body(std::string_view body_view);
};

} // namespace http_server
