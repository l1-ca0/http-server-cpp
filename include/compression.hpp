#pragma once

#include <string>
#include <vector>

namespace http_server {
namespace compression {

std::string gzip_compress(const std::string& data);
std::string gzip_decompress(const std::string& compressed_data);
bool supports_gzip(const std::string& accept_encoding);
std::vector<std::string> parse_accept_encoding(const std::string& accept_encoding);

} // namespace compression
} // namespace http_server
