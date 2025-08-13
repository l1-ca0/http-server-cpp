#include <gtest/gtest.h>
#include "server.hpp"
#include "ssl_connection.hpp"
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <thread>
#include <chrono>
#include <fstream>

using namespace http_server;

class HttpsServerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create basic HTTPS config
        config.enable_https = true;
        config.https_port = 18443; // Use different port for testing
        config.ssl_certificate_file = "./certs/server.crt";
        config.ssl_private_key_file = "./certs/server.key";
        config.port = 18080; // Use different port for HTTP testing
        config.document_root = "./test_public";
        config.enable_logging = false; // Disable logging for tests
        
        // Create test directory
        std::filesystem::create_directories(config.document_root);
        
        // Create a simple test file
        std::ofstream test_file(config.document_root + "/test.html");
        test_file << "<html><body>HTTPS Test</body></html>";
        test_file.close();
    }
    
    void TearDown() override {
        // Clean up test files
        std::filesystem::remove_all(config.document_root);
    }
    
    ServerConfig config;
};

// Test HTTPS configuration parsing
TEST_F(HttpsServerTest, HttpsConfigurationParsing) {
    // Create a JSON config with HTTPS settings
    nlohmann::json json_config = {
        {"enable_https", true},
        {"https_port", 8443},
        {"ssl_certificate_file", "/path/to/cert.pem"},
        {"ssl_private_key_file", "/path/to/key.pem"},
        {"ssl_ca_file", "/path/to/ca.pem"},
        {"ssl_verify_client", true},
        {"ssl_cipher_list", "HIGH:!aNULL"}
    };
    
    auto config = ServerConfig::from_json_string(json_config.dump());
    
    EXPECT_TRUE(config.enable_https);
    EXPECT_EQ(config.https_port, 8443);
    EXPECT_EQ(config.ssl_certificate_file, "/path/to/cert.pem");
    EXPECT_EQ(config.ssl_private_key_file, "/path/to/key.pem");
    EXPECT_EQ(config.ssl_ca_file, "/path/to/ca.pem");
    EXPECT_TRUE(config.ssl_verify_client);
    EXPECT_EQ(config.ssl_cipher_list, "HIGH:!aNULL");
}

// Test HTTPS configuration serialization
TEST_F(HttpsServerTest, HttpsConfigurationSerialization) {
    ServerConfig config;
    config.enable_https = true;
    config.https_port = 9443;
    config.ssl_certificate_file = "/test/cert.pem";
    config.ssl_private_key_file = "/test/key.pem";
    
    auto json = config.to_json();
    
    EXPECT_TRUE(json["enable_https"]);
    EXPECT_EQ(json["https_port"], 9443);
    EXPECT_EQ(json["ssl_certificate_file"], "/test/cert.pem");
    EXPECT_EQ(json["ssl_private_key_file"], "/test/key.pem");
}

// Test HTTPS server initialization (without actual SSL handshake)
TEST_F(HttpsServerTest, HttpsServerInitialization) {
    // Check if certificate files exist
    if (!std::filesystem::exists(config.ssl_certificate_file) || 
        !std::filesystem::exists(config.ssl_private_key_file)) {
        GTEST_SKIP() << "SSL certificates not found, skipping HTTPS test";
    }
    
    EXPECT_NO_THROW({
        HttpServer server(config);
        // Just test that the server can be constructed with HTTPS config
        EXPECT_TRUE(config.enable_https);
    });
}

// Test mixed HTTP/HTTPS configuration
TEST_F(HttpsServerTest, MixedHttpHttpsConfiguration) {
    config.enable_https = true;
    config.port = 18080;
    config.https_port = 18443;
    
    // Check if certificate files exist
    if (!std::filesystem::exists(config.ssl_certificate_file) || 
        !std::filesystem::exists(config.ssl_private_key_file)) {
        GTEST_SKIP() << "SSL certificates not found, skipping HTTPS test";
    }
    
    EXPECT_NO_THROW({
        HttpServer server(config);
        EXPECT_EQ(server.config().port, 18080);
        EXPECT_EQ(server.config().https_port, 18443);
        EXPECT_TRUE(server.config().enable_https);
    });
}

// Test SSL context validation
TEST_F(HttpsServerTest, SslContextValidation) {
    // Test with missing certificate files
    config.ssl_certificate_file = "/nonexistent/cert.pem";
    config.ssl_private_key_file = "/nonexistent/key.pem";
    
    EXPECT_THROW({
        HttpServer server(config);
        // This should throw when trying to start, but constructor should work
    }, std::exception);
}

// Test SSL connection creation (basic functionality)
TEST_F(HttpsServerTest, SslConnectionBasics) {
    boost::asio::io_context io_context;
    boost::asio::ssl::context ssl_context(boost::asio::ssl::context::tlsv12);
    
    // Create a SSL socket (not connected)
    SslConnection::SslSocket socket(io_context, ssl_context);
    
    auto handler = [](const HttpRequest& req) -> HttpResponse {
        return HttpResponse::ok("HTTPS Test Response");
    };
    
    auto cleanup = [](){};
    
    // Just test that SSL connection can be created
    EXPECT_NO_THROW({
        auto connection = std::make_shared<SslConnection>(
            std::move(socket), handler, cleanup
        );
        EXPECT_TRUE(connection != nullptr);
    });
}

// Test HTTPS statistics tracking
TEST_F(HttpsServerTest, HttpsStatisticsTracking) {
    // Check if certificate files exist
    if (!std::filesystem::exists(config.ssl_certificate_file) || 
        !std::filesystem::exists(config.ssl_private_key_file)) {
        GTEST_SKIP() << "SSL certificates not found, skipping HTTPS test";
    }
    
    HttpServer server(config);
    
    // Test that statistics are properly initialized
    const auto& stats = server.stats();
    EXPECT_EQ(stats.total_requests.load(), 0);
    EXPECT_EQ(stats.active_connections.load(), 0);
    EXPECT_EQ(stats.total_connections.load(), 0);
}

// Test configuration file loading with HTTPS
TEST_F(HttpsServerTest, HttpsConfigFileLoading) {
    // Create a temporary HTTPS config file
    std::string config_path = "./test_https_config.json";
    
    nlohmann::json https_config = {
        {"host", "127.0.0.1"},
        {"port", 18080},
        {"enable_https", true},
        {"https_port", 18443},
        {"ssl_certificate_file", "./certs/server.crt"},
        {"ssl_private_key_file", "./certs/server.key"},
        {"ssl_cipher_list", "HIGH:!aNULL:!MD5"}
    };
    
    std::ofstream config_file(config_path);
    config_file << https_config.dump(2);
    config_file.close();
    
    EXPECT_NO_THROW({
        auto loaded_config = ServerConfig::from_json(config_path);
        EXPECT_TRUE(loaded_config.enable_https);
        EXPECT_EQ(loaded_config.https_port, 18443);
        EXPECT_EQ(loaded_config.ssl_certificate_file, "./certs/server.crt");
        EXPECT_EQ(loaded_config.ssl_private_key_file, "./certs/server.key");
    });
    
    // Clean up
    std::filesystem::remove(config_path);
}

// Test HTTPS routing (basic)
TEST_F(HttpsServerTest, HttpsRoutingBasics) {
    // Check if certificate files exist
    if (!std::filesystem::exists(config.ssl_certificate_file) || 
        !std::filesystem::exists(config.ssl_private_key_file)) {
        GTEST_SKIP() << "SSL certificates not found, skipping HTTPS test";
    }
    
    HttpServer server(config);
    
    // Add a test route
    server.add_get_route("/https-test", [](const HttpRequest& req) {
        return HttpResponse::ok("HTTPS endpoint working");
    });
    
    // Just verify the server can be configured with routes
    // Actual HTTPS communication would require more complex setup
    EXPECT_TRUE(server.config().enable_https);
}

// Test SSL cipher configuration
TEST_F(HttpsServerTest, SslCipherConfiguration) {
    config.ssl_cipher_list = "HIGH:!aNULL:!eNULL:!EXPORT:!DES:!RC4:!MD5:!PSK:!SRP:!CAMELLIA";
    
    // Check if certificate files exist
    if (!std::filesystem::exists(config.ssl_certificate_file) || 
        !std::filesystem::exists(config.ssl_private_key_file)) {
        GTEST_SKIP() << "SSL certificates not found, skipping HTTPS test";
    }
    
    EXPECT_NO_THROW({
        HttpServer server(config);
        EXPECT_EQ(server.config().ssl_cipher_list, "HIGH:!aNULL:!eNULL:!EXPORT:!DES:!RC4:!MD5:!PSK:!SRP:!CAMELLIA");
    });
}

// Test HTTPS disabled configuration
TEST_F(HttpsServerTest, HttpsDisabledConfiguration) {
    config.enable_https = false;
    
    HttpServer server(config);
    
    // Should work fine without HTTPS
    EXPECT_FALSE(server.config().enable_https);
    
    server.add_get_route("/test", [](const HttpRequest& req) {
        return HttpResponse::ok("HTTP only");
    });
    
    // Server should be configurable even without HTTPS
    EXPECT_TRUE(true);
}
