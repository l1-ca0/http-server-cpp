/**
 * @file response.cpp
 * @brief Implementation of the HttpResponse class for constructing and serializing HTTP responses.
 *
 * Provides methods for setting status, headers, body, content type, and serializing responses to HTTP format.
 */
#include "response.hpp"
#include "compression.hpp"
#include "request.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <functional>
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
        case HttpStatus::SWITCHING_PROTOCOLS: return "Switching Protocols";
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
        case HttpStatus::TOO_MANY_REQUESTS: return "Too Many Requests";
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

HttpResponse& HttpResponse::set_etag(const std::string& etag, bool weak) {
    std::string formatted_etag = weak ? "W/\"" + etag + "\"" : "\"" + etag + "\"";
    set_header("ETag", formatted_etag);
    return *this;
}

HttpResponse& HttpResponse::set_last_modified(const std::chrono::system_clock::time_point& time) {
    set_header("Last-Modified", format_http_time(time));
    return *this;
}

HttpResponse& HttpResponse::set_last_modified(const std::string& rfc1123_time) {
    set_header("Last-Modified", rfc1123_time);
    return *this;
}

std::string HttpResponse::get_etag() const {
    return get_header("ETag");
}

std::chrono::system_clock::time_point HttpResponse::get_last_modified() const {
    auto last_modified_str = get_header("Last-Modified");
    if (last_modified_str.empty()) {
        return std::chrono::system_clock::time_point{};
    }
    return parse_http_time(last_modified_str);
}

HttpResponse HttpResponse::conditional_file_response(const std::string& file_path, const HttpRequest& request) {
    try {
        std::filesystem::path path(file_path);
        
        if (!std::filesystem::exists(path)) {
            return HttpResponse::not_found();
        }
        
        // Get file stats for Last-Modified and ETag generation
        auto file_time = std::filesystem::last_write_time(path);
        auto file_size = std::filesystem::file_size(path);
        
        // Generate ETag based on file path, size, and modification time
        // Use simple approach to avoid time conversion issues
        auto time_since_epoch = file_time.time_since_epoch().count();
        std::string etag_data = file_path + std::to_string(file_size) + 
                               std::to_string(static_cast<long long>(time_since_epoch));
        std::string etag = generate_etag(etag_data);
        
        // Convert file time to system time for Last-Modified header
        auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            file_time - std::filesystem::file_time_type::clock::now() +
            std::chrono::system_clock::now()
        );
        
        // Check conditional headers
        auto if_none_match = request.get_if_none_match();
        auto if_modified_since = request.get_if_modified_since();
        
        // Check If-None-Match header (ETag-based conditional)
        if (if_none_match && etag_matches("\"" + etag + "\"", *if_none_match)) {
            HttpResponse response(HttpStatus::NOT_MODIFIED);
            response.set_etag(etag);  // set_etag will add quotes
            response.set_last_modified(sctp);
            // For 304 responses, must explicitly set empty body and Content-Length to 0
            response.set_body("");
            return response;
        }
        
        // Check If-Modified-Since header (time-based conditional)
        if (if_modified_since && !if_modified_since->empty()) {
            // For now, skip time comparison to avoid parsing issues
            // In a real implementation, you would parse if_modified_since and compare
        }
        
        // File has been modified or no conditional headers - serve full file
        HttpResponse response = file_response(file_path);
        if (response.status() == HttpStatus::OK) {
            response.set_etag(etag);  // set_etag will add quotes
            response.set_last_modified(sctp);
            // Set appropriate cache headers
            response.set_cache_control("public, max-age=3600"); // Cache for 1 hour
        }
        
        return response;
        
    } catch (const std::exception&) {
        return HttpResponse::internal_error("Error processing conditional request");
    }
}

std::string HttpResponse::generate_etag(const std::string& content) {
    // Simple hash-based ETag generation
    std::hash<std::string> hasher;
    auto hash = hasher(content);
    
    // Convert to hex string
    std::ostringstream oss;
    oss << std::hex << hash;
    return oss.str();
}

std::string HttpResponse::generate_file_etag(const std::string& file_path) {
    try {
        std::filesystem::path path(file_path);
        if (!std::filesystem::exists(path)) {
            return "";
        }
        
        auto file_time = std::filesystem::last_write_time(path);
        auto file_size = std::filesystem::file_size(path);
        
        // Create ETag based on file metadata (more efficient than reading entire file)
        std::string etag_data = file_path + std::to_string(file_size) + 
                               std::to_string(static_cast<long long>(file_time.time_since_epoch().count()));
        
        return generate_etag(etag_data);
    } catch (const std::exception&) {
        return "";
    }
}

std::string HttpResponse::format_http_time(const std::chrono::system_clock::time_point& time) {
    auto time_t = std::chrono::system_clock::to_time_t(time);
    
    std::ostringstream oss;
    oss << std::put_time(std::gmtime(&time_t), "%a, %d %b %Y %H:%M:%S GMT");
    return oss.str();
}

std::chrono::system_clock::time_point HttpResponse::parse_http_time(const std::string& time_str) {
    // Simplified time parsing to avoid segmentation faults
    // For now, return a valid time point based on the current time
    // This is a fallback implementation to prevent crashes
    
    if (time_str.empty()) {
        return std::chrono::system_clock::time_point{};
    }
    
    // Basic RFC 1123 parsing without using get_time which can cause issues
    // Just return a reasonable default time for now
    auto now = std::chrono::system_clock::now();
    return now;
}

bool HttpResponse::etag_matches(const std::string& etag, const std::string& if_none_match) {
    // Handle "*" which matches any ETag
    if (if_none_match == "*") {
        return true;
    }
    
    // Parse multiple ETags separated by commas
    std::istringstream iss(if_none_match);
    std::string token;
    
    while (std::getline(iss, token, ',')) {
        // Trim whitespace
        token.erase(0, token.find_first_not_of(" \t"));
        token.erase(token.find_last_not_of(" \t") + 1);
        
        // Compare ETags (handle both weak and strong)
        if (token == etag) {
            return true;
        }
        
        // Check if this is a weak ETag comparison
        std::string clean_etag = etag;
        std::string clean_token = token;
        
        // Remove W/ prefix if present
        if (clean_etag.substr(0, 2) == "W/") {
            clean_etag = clean_etag.substr(2);
        }
        if (clean_token.substr(0, 2) == "W/") {
            clean_token = clean_token.substr(2);
        }
        
        if (clean_token == clean_etag) {
            return true;
        }
    }
    
    return false;
}

std::string HttpResponse::format_timestamp() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::ostringstream oss;
    oss << std::put_time(std::gmtime(&time_t), "%a, %d %b %Y %H:%M:%S GMT");
    return oss.str();
}

} // namespace http_server
