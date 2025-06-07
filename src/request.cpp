/**
 * @file request.cpp
 * @brief Implementation of the HttpRequest class for parsing and representing HTTP requests.
 *
 * Provides parsing logic for HTTP request lines, headers, query parameters, and body extraction.
 */
#include "request.hpp"
#include <sstream>
#include <algorithm>
#include <cctype>

namespace http_server {

std::optional<HttpRequest> HttpRequest::parse(std::string_view raw_request) {
    HttpRequest request;
    if (raw_request.empty()) {
        return std::nullopt;
    }

    // Find the end of the headers
    size_t header_end_pos = raw_request.find("\r\n\r\n");
    if (header_end_pos == std::string_view::npos) {
        // Fallback for headers ending with \n\n
        header_end_pos = raw_request.find("\n\n");
        if (header_end_pos == std::string_view::npos) {
            return std::nullopt; // Headers not terminated
        }
    }

    std::string header_str(raw_request.substr(0, header_end_pos));
    std::istringstream header_stream(header_str);
    std::string line;

    // Parse request line
    if (std::getline(header_stream, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        request.parse_request_line(line);
    } else {
        return std::nullopt;
    }

    // Parse headers
    while (std::getline(header_stream, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty()) {
            continue; // Should not happen with this logic, but safe
        }
        request.parse_header_line(line);
    }

    // The body starts after the header separator
    size_t body_start_pos = raw_request.find("\r\n\r\n") != std::string::npos ? header_end_pos + 4 : header_end_pos + 2;
    std::string_view body_view = raw_request.substr(body_start_pos);

    // Body Parsing
    auto transfer_encoding = request.get_header("transfer-encoding");
    if (transfer_encoding && transfer_encoding->find("chunked") != std::string::npos) {
        if (!request.parse_chunked_body(body_view)) {
            return std::nullopt;
        }
    }
    else {
        size_t content_length = request.content_length();
        if (content_length > 0) {
            if (body_view.length() < content_length) {
                // This indicates an incomplete request, which the connection manager should handle.
                // For this parser, we treat it as an error for now.
                return std::nullopt;
            }
            if (content_length > 10 * 1024 * 1024) { // 10MB limit for safety
                return std::nullopt; 
            }
            request.body_ = std::string(body_view.substr(0, content_length));
        }
    }

    request.is_valid_ = (request.method_ != HttpMethod::UNKNOWN &&
                          !request.path_.empty() &&
                          is_valid_http_version(request.version_));

    return request.is_valid_ ? std::make_optional(request) : std::nullopt;
}

void HttpRequest::parse_request_line(std::string_view line) {
    std::istringstream iss{std::string{line}};
    std::string method_str, path_and_query, version;
    
    if (!(iss >> method_str >> path_and_query >> version)) {
        return;
    }
    
    method_ = string_to_method(method_str);
    version_ = version;
    
    // Parse path and query string
    auto query_pos = path_and_query.find('?');
    if (query_pos != std::string::npos) {
        path_ = path_and_query.substr(0, query_pos);
        parse_query_string(path_and_query.substr(query_pos + 1));
    } else {
        path_ = path_and_query;
    }
}

void HttpRequest::parse_header_line(std::string_view line) {
    auto colon_pos = line.find(':');
    if (colon_pos == std::string_view::npos) {
        return;
    }
    
    std::string name{line.substr(0, colon_pos)};
    std::string value{line.substr(colon_pos + 1)};
    
    // Trim whitespace
    name.erase(name.find_last_not_of(" \t") + 1);
    value.erase(0, value.find_first_not_of(" \t"));
    value.erase(value.find_last_not_of(" \t") + 1);
    
    // Validate header name to prevent injection attacks
    // RFC 7230: header names must be tokens (visible ASCII chars except separators)
    if (!is_valid_header_name(name)) {
        return; // Reject invalid header names
    }
    
    // Validate header value to prevent injection attacks
    // RFC 7230: header values cannot contain control characters except HTAB
    if (!is_valid_header_value(value)) {
        return; // Reject invalid header values
    }
    
    normalize_header_name(name);
    headers_[name] = value;
}

void HttpRequest::parse_query_string(std::string_view query) {
    std::string query_str{query};
    std::istringstream iss(query_str);
    std::string pair;
    
    while (std::getline(iss, pair, '&')) {
        auto eq_pos = pair.find('=');
        if (eq_pos != std::string::npos) {
            std::string key = pair.substr(0, eq_pos);
            std::string value = pair.substr(eq_pos + 1);
            query_params_[key] = value;
        } else {
            query_params_[pair] = "";
        }
    }
}

void HttpRequest::normalize_header_name(std::string& name) const {
    std::transform(name.begin(), name.end(), name.begin(), ::tolower);
}

std::optional<std::string> HttpRequest::get_header(const std::string& name) const {
    std::string normalized_name = name;
    normalize_header_name(normalized_name);
    
    auto it = headers_.find(normalized_name);
    return (it != headers_.end()) ? std::make_optional(it->second) : std::nullopt;
}

bool HttpRequest::has_header(const std::string& name) const {
    std::string normalized_name = name;
    normalize_header_name(normalized_name);
    return headers_.find(normalized_name) != headers_.end();
}

size_t HttpRequest::content_length() const {
    auto length_header = get_header("content-length");
    if (length_header) {
        try {
            return std::stoull(*length_header);
        } catch (const std::exception&) {
            return 0;
        }
    }
    return 0;
}

std::string HttpRequest::content_type() const {
    auto type_header = get_header("content-type");
    return type_header ? *type_header : "";
}

std::optional<std::string> HttpRequest::get_query_param(const std::string& name) const {
    auto it = query_params_.find(name);
    return (it != query_params_.end()) ? std::make_optional(it->second) : std::nullopt;
}

bool HttpRequest::has_query_param(const std::string& name) const {
    return query_params_.find(name) != query_params_.end();
}

bool HttpRequest::is_keep_alive() const {
    auto connection_header = get_header("connection");
    if (connection_header) {
        std::string conn = *connection_header;
        std::transform(conn.begin(), conn.end(), conn.begin(), ::tolower);
        return conn == "keep-alive";
    }
    
    // HTTP/1.1 defaults to keep-alive
    return version_ == "HTTP/1.1";
}

std::string HttpRequest::to_string() const {
    std::ostringstream oss;
    oss << method_to_string(method_) << " " << path_;
    
    if (!query_params_.empty()) {
        oss << "?";
        bool first = true;
        for (const auto& [key, value] : query_params_) {
            if (!first) oss << "&";
            oss << key << "=" << value;
            first = false;
        }
    }
    
    oss << " " << version_ << "\r\n";
    
    for (const auto& [name, value] : headers_) {
        oss << name << ": " << value << "\r\n";
    }
    
    oss << "\r\n" << body_;
    
    return oss.str();
}

std::string HttpRequest::method_to_string(HttpMethod method) {
    switch (method) {
        case HttpMethod::GET: return "GET";
        case HttpMethod::POST: return "POST";
        case HttpMethod::PUT: return "PUT";
        case HttpMethod::DELETE: return "DELETE";
        case HttpMethod::HEAD: return "HEAD";
        case HttpMethod::OPTIONS: return "OPTIONS";
        case HttpMethod::PATCH: return "PATCH";
        default: return "UNKNOWN";
    }
}

HttpMethod HttpRequest::string_to_method(std::string_view method_str) {
    if (method_str == "GET") return HttpMethod::GET;
    if (method_str == "POST") return HttpMethod::POST;
    if (method_str == "PUT") return HttpMethod::PUT;
    if (method_str == "DELETE") return HttpMethod::DELETE;
    if (method_str == "HEAD") return HttpMethod::HEAD;
    if (method_str == "OPTIONS") return HttpMethod::OPTIONS;
    if (method_str == "PATCH") return HttpMethod::PATCH;
    return HttpMethod::UNKNOWN;
}

bool HttpRequest::is_valid_header_name(const std::string& name) {
    if (name.empty()) {
        return false;
    }
    
    // RFC 7230: header names are tokens
    // token = 1*tchar
    // tchar = "!" / "#" / "$" / "%" / "&" / "'" / "*" / "+" / "-" / "." /
    //         "^" / "_" / "`" / "|" / "~" / DIGIT / ALPHA
    for (char c : name) {
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || 
              (c >= '0' && c <= '9') || 
              c == '!' || c == '#' || c == '$' || c == '%' || c == '&' || 
              c == '\'' || c == '*' || c == '+' || c == '-' || c == '.' || 
              c == '^' || c == '_' || c == '`' || c == '|' || c == '~')) {
            return false;
        }
    }
    return true;
}

bool HttpRequest::is_valid_header_value(const std::string& value) {
    // RFC 7230: header values are field-content
    // field-content = field-vchar [ 1*( SP / HTAB ) field-vchar ]
    // field-vchar = VCHAR / obs-text
    // VCHAR = %x21-7E (visible ASCII characters)
    // obs-text = %x80-FF (for compatibility)
    for (char c : value) {
        unsigned char uc = static_cast<unsigned char>(c);
        // Allow visible ASCII chars (0x21-0x7E), space (0x20), tab (0x09)
        // Reject control characters especially CR (0x0D) and LF (0x0A)
        if (!((uc >= 0x21 && uc <= 0x7E) || uc == 0x20 || uc == 0x09)) {
            // Allow extended ASCII for compatibility (0x80-0xFF)
            if (!(uc >= 0x80 && uc <= 0xFF)) {
                return false;
            }
        }
    }
    return true;
}

bool HttpRequest::parse_chunked_body(std::string_view body_view) {
    std::ostringstream body_stream;
    size_t pos = 0;
    size_t total_body_size = 0;
    const size_t MAX_BODY_SIZE = 10 * 1024 * 1024; // 10MB safety limit

    while (pos < body_view.length()) {
        // Find the end of the chunk size line
        size_t line_end = body_view.find("\r\n", pos);
        if (line_end == std::string_view::npos) {
            return false; // Malformed chunk size line
        }

        // Parse chunk size
        std::string size_hex = std::string(body_view.substr(pos, line_end - pos));
        
        // Remove any chunk extensions (semicolon and beyond)
        auto semicolon_pos = size_hex.find(';');
        if (semicolon_pos != std::string::npos) {
            size_hex = size_hex.substr(0, semicolon_pos);
        }
        
        size_t chunk_size;
        try {
            chunk_size = std::stoul(size_hex, nullptr, 16);
        } catch (const std::exception&) {
            return false; // Invalid chunk size format
        }

        pos = line_end + 2;

        if (chunk_size == 0) {
            // End of chunks. Look for optional trailer headers.
            // This simplified parser does not process trailer headers but ensures we stop correctly.
            break;
        }

        // Safety check for total body size
        total_body_size += chunk_size;
        if (total_body_size > MAX_BODY_SIZE) {
            return false; // Body too large
        }

        // Check if we have enough data for the chunk
        if (pos + chunk_size > body_view.length()) {
            return false; // Incomplete chunk data
        }

        // Append chunk data to the body
        body_stream << body_view.substr(pos, chunk_size);
        pos += chunk_size;

        // Skip the trailing CRLF after the chunk data
        if (pos + 2 > body_view.length() || body_view.substr(pos, 2) != "\r\n") {
            return false; // Expected CRLF after chunk data
        }
        pos += 2;
    }

    body_ = body_stream.str();
    return true;
}

bool HttpRequest::is_valid_http_version(const std::string& version) {
    // Only accept HTTP/1.0 and HTTP/1.1 versions
    return version == "HTTP/1.0" || version == "HTTP/1.1";
}

} // namespace http_server