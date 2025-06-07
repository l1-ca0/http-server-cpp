/**
 * @file response.cpp
 * @brief Implementation of the HttpResponse class for constructing and serializing HTTP responses.
 *
 * Provides methods for setting status, headers, body, content type, and serializing responses to HTTP format.
 */
#include "response.hpp"
#include "compression.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <algorithm>
#include <cctype>

namespace http_server {

HttpResponse::HttpResponse() {
    set_default_headers();
}

HttpResponse::HttpResponse(HttpStatus status) : status_(status) {
    set_default_headers();
}

HttpResponse& HttpResponse::set_status(HttpStatus status) {
    status_ = status;
    return *this;
}

HttpResponse& HttpResponse::set_header(const std::string& name, const std::string& value) {
    std::string normalized_name = name;
    normalize_header_name(normalized_name);
    headers_[normalized_name] = value;
    return *this;
}

HttpResponse& HttpResponse::add_header(const std::string& name, const std::string& value) {
    std::string normalized_name = name;
    normalize_header_name(normalized_name);
    
    auto it = headers_.find(normalized_name);
    if (it != headers_.end()) {
        it->second += ", " + value;
    } else {
        headers_[normalized_name] = value;
    }
    return *this;
}

std::string HttpResponse::get_header(const std::string& name) const {
    std::string normalized_name = name;
    normalize_header_name(normalized_name);
    
    auto it = headers_.find(normalized_name);
    return (it != headers_.end()) ? it->second : "";
}

bool HttpResponse::has_header(const std::string& name) const {
    std::string normalized_name = name;
    normalize_header_name(normalized_name);
    return headers_.find(normalized_name) != headers_.end();
}

HttpResponse& HttpResponse::remove_header(const std::string& name) {
    std::string normalized_name = name;
    normalize_header_name(normalized_name);
    headers_.erase(normalized_name);
    return *this;
}

HttpResponse& HttpResponse::set_body(const std::string& body) {
    body_content_ = body;
    body_stream_ = std::make_shared<std::stringstream>(body_content_);
    set_header("Content-Length", std::to_string(body_content_.size()));
    return *this;
}

HttpResponse& HttpResponse::set_body(std::string&& body) {
    body_content_ = std::move(body);
    body_stream_ = std::make_shared<std::stringstream>(body_content_);
    set_header("Content-Length", std::to_string(body_content_.size()));
    return *this;
}

HttpResponse& HttpResponse::set_content_type(const std::string& content_type) {
    set_header("Content-Type", content_type);
    return *this;
}

HttpResponse& HttpResponse::set_json(const std::string& json_data) {
    set_content_type("application/json; charset=utf-8");
    set_body(json_data);
    return *this;
}

HttpResponse& HttpResponse::set_html(const std::string& html_content) {
    set_content_type("text/html; charset=utf-8");
    set_body(html_content);
    return *this;
}

HttpResponse& HttpResponse::set_text(const std::string& text_content) {
    set_content_type("text/plain; charset=utf-8");
    set_body(text_content);
    return *this;
}

HttpResponse& HttpResponse::set_file_content(const std::string& file_path) {
    try {
        std::ifstream file_stream(file_path, std::ios::binary);

        if (!file_stream.is_open()) {
            set_status(HttpStatus::NOT_FOUND);
            set_text("File not found");
            return *this;
        }

        // Read the entire file content into body_content_
        std::string content((std::istreambuf_iterator<char>(file_stream)),
                           std::istreambuf_iterator<char>());
        
        body_content_ = std::move(content);
        set_header("Content-Length", std::to_string(body_content_.size()));

        // Get file extension for MIME type
        std::filesystem::path path(file_path);
        std::string extension = path.extension().string();
        if (!extension.empty() && extension[0] == '.') {
            extension = extension.substr(1);
        }
        
        std::string mime_type = get_mime_type(extension);
        set_content_type(mime_type);
        
    } catch (const std::exception&) {
        set_status(HttpStatus::INTERNAL_SERVER_ERROR);
        set_text("Error reading file");
    }
    
    return *this;
}

HttpResponse& HttpResponse::set_keep_alive(bool keep_alive) {
    set_header("Connection", keep_alive ? "keep-alive" : "close");
    return *this;
}

HttpResponse& HttpResponse::set_cache_control(const std::string& cache_control) {
    set_header("Cache-Control", cache_control);
    return *this;
}

HttpResponse& HttpResponse::set_cors_headers(const std::string& origin) {
    set_header("Access-Control-Allow-Origin", origin);
    set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
    set_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
    return *this;
}

HttpResponse& HttpResponse::set_compressed_body(const std::string& body, const std::string& encoding) {
    if (encoding == "gzip") {
        std::string compressed = compression::gzip_compress(body);
        if (!compressed.empty()) {
            set_body(compressed);
            set_header("Content-Encoding", "gzip");
        } else {
            // Fallback to uncompressed if compression fails
            set_body(body);
        }
    } else {
        set_body(body);
    }
    return *this;
}

HttpResponse& HttpResponse::compress_body_if_supported(const std::string& accept_encoding) {
    if (compression::supports_gzip(accept_encoding) && !body_content_.empty() && !is_compressed()) {
        // Check if content type is compressible (basic check)
        std::string content_type = get_header("Content-Type");
        if (content_type.find("text/") == 0 || 
            content_type.find("application/json") == 0 ||
            content_type.find("application/javascript") == 0 ||
            content_type.find("application/xml") == 0) {
            
            // Only compress if body is large enough (avoid overhead for small responses)
            if (body_content_.size() >= 1024) {
                std::string compressed = compression::gzip_compress(body_content_);
                if (!compressed.empty() && compressed.size() < body_content_.size()) {
                    set_body(compressed);
                    set_header("Content-Encoding", "gzip");
                }
            }
        }
    }
    return *this;
}

bool HttpResponse::is_compressed() const {
    return has_header("Content-Encoding");
}

std::string HttpResponse::to_string() const {
    std::ostringstream oss;
    oss << "Status: " << static_cast<int>(status_) << " " << get_status_message(status_) << "\n";
    
    for (const auto& [name, value] : headers_) {
        oss << name << ": " << value << "\n";
    }
    
    if (!body_content_.empty()) {
        oss << "\nBody (" << body_content_.size() << " bytes):\n" << body_content_;
    }
    
    return oss.str();
}

std::string HttpResponse::to_http_string() const {
    std::ostringstream oss;
    oss << version_ << " " << static_cast<int>(status_) << " " << get_status_message(status_) << "\r\n";
    
    for (const auto& [name, value] : headers_) {
        oss << name << ": " << value << "\r\n";
    }
    
    oss << "\r\n";
    
    // Include the body content if available
    if (!body_content_.empty()) {
        oss << body_content_;
    }
    
    return oss.str();
}

HttpResponse HttpResponse::ok(const std::string& body) {
    HttpResponse response(HttpStatus::OK);
    if (!body.empty()) {
        response.set_text(body);
    }
    return response;
}

HttpResponse HttpResponse::not_found(const std::string& message) {
    HttpResponse response(HttpStatus::NOT_FOUND);
    response.set_text(message);
    return response;
}

HttpResponse HttpResponse::bad_request(const std::string& message) {
    HttpResponse response(HttpStatus::BAD_REQUEST);
    response.set_text(message);
    return response;
}

HttpResponse HttpResponse::internal_error(const std::string& message) {
    HttpResponse response(HttpStatus::INTERNAL_SERVER_ERROR);
    response.set_text(message);
    return response;
}

HttpResponse HttpResponse::json_response(const std::string& json_data, HttpStatus status) {
    HttpResponse response(status);
    response.set_json(json_data);
    return response;
}

HttpResponse HttpResponse::file_response(const std::string& file_path) {
    HttpResponse response(HttpStatus::OK);
    response.set_file_content(file_path);
    return response;
}

std::string HttpResponse::get_mime_type(const std::string& file_extension) {
    static const std::unordered_map<std::string, std::string> mime_types = {
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
        {"ico", "image/x-icon"},
        {"pdf", "application/pdf"},
        {"zip", "application/zip"},
        {"gz", "application/gzip"},
        {"mp4", "video/mp4"},
        {"mp3", "audio/mpeg"},
        {"wav", "audio/wav"},
        {"woff", "font/woff"},
        {"woff2", "font/woff2"},
        {"ttf", "font/ttf"},
        {"eot", "application/vnd.ms-fontobject"}
    };
    
    std::string lower_ext = file_extension;
    std::transform(lower_ext.begin(), lower_ext.end(), lower_ext.begin(), ::tolower);
    
    auto it = mime_types.find(lower_ext);
    return (it != mime_types.end()) ? it->second : "application/octet-stream";
}

std::string HttpResponse::get_status_message(HttpStatus status) {
    switch (status) {
        case HttpStatus::OK: return "OK";
        case HttpStatus::CREATED: return "Created";
        case HttpStatus::ACCEPTED: return "Accepted";
        case HttpStatus::NO_CONTENT: return "No Content";
        case HttpStatus::MOVED_PERMANENTLY: return "Moved Permanently";
        case HttpStatus::FOUND: return "Found";
        case HttpStatus::NOT_MODIFIED: return "Not Modified";
        case HttpStatus::BAD_REQUEST: return "Bad Request";
        case HttpStatus::UNAUTHORIZED: return "Unauthorized";
        case HttpStatus::FORBIDDEN: return "Forbidden";
        case HttpStatus::NOT_FOUND: return "Not Found";
        case HttpStatus::METHOD_NOT_ALLOWED: return "Method Not Allowed";
        case HttpStatus::CONFLICT: return "Conflict";
        case HttpStatus::LENGTH_REQUIRED: return "Length Required";
        case HttpStatus::PAYLOAD_TOO_LARGE: return "Payload Too Large";
        case HttpStatus::INTERNAL_SERVER_ERROR: return "Internal Server Error";
        case HttpStatus::NOT_IMPLEMENTED: return "Not Implemented";
        case HttpStatus::BAD_GATEWAY: return "Bad Gateway";
        case HttpStatus::SERVICE_UNAVAILABLE: return "Service Unavailable";
        default: return "Unknown";
    }
}

void HttpResponse::set_default_headers() {
    set_header("Server", "cpp-http-server/1.0");
    set_header("Date", format_timestamp());
    set_header("Content-Length", "0");
}

void HttpResponse::normalize_header_name(std::string& name) const {
    // Convert to proper case (e.g., "content-type" -> "Content-Type")
    bool capitalize_next = true;
    for (char& c : name) {
        if (c == '-') {
            capitalize_next = true;
        } else if (capitalize_next) {
            c = std::toupper(c);
            capitalize_next = false;
        } else {
            c = std::tolower(c);
        }
    }
}

std::string HttpResponse::format_timestamp() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::ostringstream oss;
    oss << std::put_time(std::gmtime(&time_t), "%a, %d %b %Y %H:%M:%S GMT");
    return oss.str();
}

} // namespace http_server
