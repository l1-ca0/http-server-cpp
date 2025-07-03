/**
 * @file compression.cpp
 * @brief Implementation of compression utilities for HTTP responses using ZLIB
 */
#include "compression.hpp"
#include <zlib.h>
#include <cstring>
#include <sstream>
#include <algorithm>

namespace http_server {
namespace compression {

std::string gzip_compress(const std::string& data) {
    if (data.empty()) {
        return "";
    }
    
    z_stream zs;
    memset(&zs, 0, sizeof(zs));
    
    if (deflateInit2(&zs, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 
                    15 + 16, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
        return "";
    }
    
    zs.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(data.data()));
    zs.avail_in = data.size();
    
    int ret;
    char outbuffer[32768];
    std::string outstring;
    
    do {
        zs.next_out = reinterpret_cast<Bytef*>(outbuffer);
        zs.avail_out = sizeof(outbuffer);
        
        ret = deflate(&zs, Z_FINISH);
        
        if (outstring.size() < zs.total_out) {
            outstring.append(outbuffer, zs.total_out - outstring.size());
        }
    } while (ret == Z_OK);
    
    deflateEnd(&zs);
    
    if (ret != Z_STREAM_END) {
        return "";
    }
    
    return outstring;
}

std::string gzip_decompress(const std::string& compressed_data) {
    if (compressed_data.empty()) {
        return "";
    }
    
    z_stream zs;
    memset(&zs, 0, sizeof(zs));
    
    if (inflateInit2(&zs, 15 + 16) != Z_OK) {
        return "";
    }
    
    zs.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(compressed_data.data()));
    zs.avail_in = compressed_data.size();
    
    int ret;
    char outbuffer[32768];
    std::string outstring;
    
    do {
        zs.next_out = reinterpret_cast<Bytef*>(outbuffer);
        zs.avail_out = sizeof(outbuffer);
        
        ret = inflate(&zs, 0);
        
        if (outstring.size() < zs.total_out) {
            outstring.append(outbuffer, zs.total_out - outstring.size());
        }
    } while (ret == Z_OK);
    
    inflateEnd(&zs);
    
    if (ret != Z_STREAM_END) {
        return "";
    }
    
    return outstring;
}

bool supports_gzip(const std::string& accept_encoding) {
    std::string lower_encoding = accept_encoding;
    std::transform(lower_encoding.begin(), lower_encoding.end(), 
                   lower_encoding.begin(), ::tolower);
    return lower_encoding.find("gzip") != std::string::npos;
}

std::vector<std::string> parse_accept_encoding(const std::string& accept_encoding) {
    std::vector<std::string> encodings;
    std::istringstream stream(accept_encoding);
    std::string token;
    
    while (std::getline(stream, token, ',')) {
        // Trim whitespace
        token.erase(0, token.find_first_not_of(" \t"));
        token.erase(token.find_last_not_of(" \t") + 1);
        
        // Remove quality values (e.g., "gzip;q=0.8" -> "gzip")
        auto semicolon_pos = token.find(';');
        if (semicolon_pos != std::string::npos) {
            token = token.substr(0, semicolon_pos);
        }
        
        if (!token.empty()) {
            encodings.push_back(token);
        }
    }
    
    return encodings;
}

} // namespace compression
} // namespace http_server
