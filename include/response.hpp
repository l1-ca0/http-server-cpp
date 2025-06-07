#pragma once

#include <string>
#include <unordered_map>
#include <chrono>
#include <string_view>
#include <memory>
#include <istream>

namespace http_server {

enum class HttpStatus {
    OK = 200,
    CREATED = 201,
    ACCEPTED = 202,
    NO_CONTENT = 204,
    MOVED_PERMANENTLY = 301,
    FOUND = 302,
    NOT_MODIFIED = 304,
    BAD_REQUEST = 400,
    UNAUTHORIZED = 401,
    FORBIDDEN = 403,
    NOT_FOUND = 404,
    METHOD_NOT_ALLOWED = 405,
    CONFLICT = 409,
    LENGTH_REQUIRED = 411,
    PAYLOAD_TOO_LARGE = 413,
    INTERNAL_SERVER_ERROR = 500,
    NOT_IMPLEMENTED = 501,
    BAD_GATEWAY = 502,
    SERVICE_UNAVAILABLE = 503
};

class HttpResponse {
public:
    HttpResponse();
    explicit HttpResponse(HttpStatus status);
    ~HttpResponse() = default;
    
    HttpResponse& set_status(HttpStatus status);
    HttpStatus status() const noexcept { return status_; }
    
    HttpResponse& set_header(const std::string& name, const std::string& value);
    HttpResponse& add_header(const std::string& name, const std::string& value);
    std::string get_header(const std::string& name) const;
    bool has_header(const std::string& name) const;
    HttpResponse& remove_header(const std::string& name);
    
    HttpResponse& set_body(const std::string& body);
    HttpResponse& set_body(std::string&& body);
    const std::string& body() const noexcept { return body_content_; }
    std::shared_ptr<std::istream> body_stream() const { return body_stream_; }

    HttpResponse& set_content_type(const std::string& content_type);
    HttpResponse& set_json(const std::string& json_data);
    HttpResponse& set_html(const std::string& html_content);
    HttpResponse& set_text(const std::string& text_content);
    HttpResponse& set_file_content(const std::string& file_path);
    
    HttpResponse& set_keep_alive(bool keep_alive = true);
    HttpResponse& set_cache_control(const std::string& cache_control);
    HttpResponse& set_cors_headers(const std::string& origin = "*");
    
    HttpResponse& set_compressed_body(const std::string& body, const std::string& encoding = "gzip");
    HttpResponse& compress_body_if_supported(const std::string& accept_encoding);
    bool is_compressed() const;
    
    std::string to_string() const;
    std::string to_http_string() const;
    
    static HttpResponse ok(const std::string& body = "");
    static HttpResponse not_found(const std::string& message = "Not Found");
    static HttpResponse bad_request(const std::string& message = "Bad Request");
    static HttpResponse internal_error(const std::string& message = "Internal Server Error");
    static HttpResponse json_response(const std::string& json_data, HttpStatus status = HttpStatus::OK);
    static HttpResponse file_response(const std::string& file_path);
    
    static std::string get_mime_type(const std::string& file_extension);
    static std::string get_status_message(HttpStatus status);

private:
    HttpStatus status_{HttpStatus::OK};
    std::unordered_map<std::string, std::string> headers_;
    std::string body_content_;
    std::shared_ptr<std::istream> body_stream_;
    std::string version_{"HTTP/1.1"};
    
    void set_default_headers();
    void normalize_header_name(std::string& name) const;
    std::string format_timestamp() const;
};

} // namespace http_server
