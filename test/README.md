# Test Suite Documentation

This directory contains comprehensive unit tests for the C++ HTTP Server implementation using Google Test framework.

## Overview

- **Total Test Cases**: 138 tests across 10 test suites (all passing)
- **Testing Framework**: Google Test (gtest)
- **Coverage**: Core HTTP server functionality including request parsing, response generation, server configuration, thread pool management, security testing, performance validation, comprehensive HTTP/1.1 protocol compliance including chunked transfer encoding, full HTTPS/SSL support, complete WebSocket implementation with RFC 6455 compliance, advanced rate limiting with multiple algorithms, and ETag conditional request handling

## Quick Start

### Running All Tests
```bash
# Using the build script (recommended)
./scripts/build.sh debug --tests

# Or manually with CMake
mkdir -p build && cd build
cmake -DBUILD_TESTING=ON ..
cmake --build .
./test_runner

# Or build with debug configuration (automatically enables testing)
cmake -DCMAKE_BUILD_TYPE=Debug ..
cmake --build .
./test_runner
```

### Running Specific Test Suites
```bash
# Run only HttpServerTest suite
./test_runner --gtest_filter="HttpServerTest.*"

# Run only HttpRequestTest suite  
./test_runner --gtest_filter="HttpRequestTest.*"

# Run only HttpResponseTest suite
./test_runner --gtest_filter="HttpResponseTest.*"

# Run only SecurityTest suite
./test_runner --gtest_filter="SecurityTest.*"

# Run only PerformanceTest suite
./test_runner --gtest_filter="PerformanceTest.*"

# Run only HttpProtocolTest suite
./test_runner --gtest_filter="HttpProtocolTest.*"

# Run only HttpsServerTest suite
./test_runner --gtest_filter="HttpsServerTest.*"

# Run only WebSocketTest suite
./test_runner --gtest_filter="WebSocketTest.*"

# Run only RateLimiterTest suite
./test_runner --gtest_filter="RateLimiterTest.*"

# Run only ETagTest suite
./test_runner --gtest_filter="ETagTest.*"
```

### Running Individual Tests
```bash
# Run a specific test
./test_runner --gtest_filter="HttpServerTest.ServerConfigDefaults"

# Run tests matching a pattern
./test_runner --gtest_filter="*Config*"

# Run only HTTPS-related tests
./test_runner --gtest_filter="*Https*"

# Run SSL-specific functionality tests
./test_runner --gtest_filter="*Ssl*"

# Run only WebSocket-related tests
./test_runner --gtest_filter="*WebSocket*"

# Run only rate limiting tests
./test_runner --gtest_filter="*RateLimit*"
```

## Test Suites

### 1. HttpServerTest (22 tests) 
**File**: `test_server.cpp` 

Tests the core HTTP server functionality, configuration management, and thread pool operations.

#### Configuration Tests (7 tests)
- `ServerConfigDefaults` - Validates default server configuration values
- `ServerConfigFromJson` - Tests JSON configuration file parsing and loading
- `ServerConfigToJson` - Tests configuration serialization to JSON format
- `ConfigurationUpdate` - Tests runtime configuration updates
- `ConfigFileHandling` - Tests reading configuration from file
- `ConfigFileNotFound` - Tests handling of missing configuration files
- `InvalidJsonConfig` - Tests error handling for malformed JSON configuration

#### Server Lifecycle Tests (4 tests)
- `ServerCreation` - Tests HTTP server instantiation and initialization
- `RouteRegistration` - Tests adding and managing HTTP routes
- `MultipleRouteTypes` - Tests registration of different HTTP method routes
- `MiddlewareRegistration` - Tests middleware pipeline setup and registration

#### Static File & MIME Tests (3 tests)
- `StaticFileConfiguration` - Tests static file serving configuration
- `MimeTypeConfiguration` - Tests MIME type mapping configuration
- `DefaultMimeTypeInitialization` - Tests default MIME type setup

#### Statistics Tests (2 tests)
- `StatisticsInitialization` - Tests server statistics system initialization
- `StatisticsJsonSerialization` - Tests statistics data serialization to JSON

#### Thread Pool Tests (6 tests)
- `ThreadPoolInitialization` - Tests thread pool creation and setup
- `ThreadPoolTaskExecution` - Tests basic task execution in thread pool
- `ThreadPoolTaskWithParameters` - Tests parameterized task execution
- `ThreadPoolException` - Tests exception handling in thread pool tasks
- `ThreadPoolShutdown` - Tests proper thread pool shutdown and cleanup
- `ThreadPoolStoppedEnqueue` - Tests task submission to stopped thread pool

### 2. HttpRequestTest (12 tests)
**File**: `test_request.cpp` 

Tests HTTP request parsing, validation, and data extraction functionality.

#### Request Parsing Tests (4 tests)
- `ParseSimpleGetRequest` - Tests parsing basic GET requests
- `ParsePostRequestWithBody` - Tests parsing POST requests with body content
- `ParseRequestWithQueryParams` - Tests URL query parameter extraction
- `ParseDifferentHttpMethods` - Tests support for various HTTP methods (GET, POST, PUT, DELETE, etc.)

#### Method Handling Tests (2 tests)
- `MethodToStringConversion` - Tests converting HTTP method enums to strings
- `StringToMethodConversion` - Tests parsing HTTP method strings to enums

#### Header Processing Tests (2 tests)
- `HeaderCaseInsensitivity` - Tests case-insensitive header name handling
- `LargeHeaderValues` - Tests handling of headers with large values

#### Connection & Validation Tests (4 tests)
- `KeepAliveDetection` - Tests HTTP keep-alive connection detection
- `InvalidRequests` - Tests error handling for malformed HTTP requests
- `ToStringRoundTrip` - Tests request serialization and deserialization
- `SpecialCharactersInPath` - Tests URL path handling with special characters

### 3. HttpResponseTest (17 tests)
**File**: `test_response.cpp` 

Tests HTTP response generation, header management, and content handling.

#### Response Construction Tests (3 tests)
- `DefaultConstructor` - Tests default HTTP response initialization
- `StatusConstructor` - Tests response creation with specific status codes
- `SetAndGetStatus` - Tests status code setting and retrieval

#### Header Management Tests (3 tests)
- `HeaderManagement` - Tests adding, updating, and removing response headers
- `HeaderCaseNormalization` - Tests header name case normalization
- `SpecialHeaders` - Tests handling of special HTTP headers (Content-Length, etc.)

#### Content & Body Tests (4 tests)
- `BodyManagement` - Tests response body setting and retrieval
- `ContentTypeHelpers` - Tests content type header management utilities
- `LargeBodyHandling` - Tests handling of large response bodies
- `EmptyBodyHandling` - Tests handling of empty response bodies

#### File & MIME Tests (2 tests)
- `FileContent` - Tests serving file content as HTTP response
- `MimeTypeDetection` - Tests automatic MIME type detection for files

#### Utility & Factory Tests (3 tests)
- `StaticFactoryMethods` - Tests convenience methods for creating common responses
- `StatusMessages` - Tests HTTP status code to message mapping
- `FluentInterface` - Tests method chaining for response building

#### Output & Serialization Tests (2 tests)
- `HttpStringGeneration` - Tests conversion of response to HTTP protocol string
- `ToStringDebugOutput` - Tests debug string representation of responses

### 4. SecurityTest (9 tests)
**File**: `test_security.cpp`

Tests security aspects of the HTTP server including protection against common vulnerabilities.

#### Path & Request Security Tests (3 tests)
- `PathTraversalPrevention` - Tests protection against directory traversal attacks
- `RequestSizeLimits` - Tests enforcement of request size limits to prevent DoS
- `HeaderInjectionPrevention` - Tests prevention of HTTP header injection attacks through CRLF validation

#### Protocol Security Tests (3 tests)
- `HttpMethodSecurity` - Tests handling of potentially dangerous HTTP methods
- `UrlEncodingSecurity` - Tests proper URL encoding and decoding security
- `ContentTypeValidation` - Tests validation of Content-Type headers

#### Security Headers & Policies Tests (3 tests)
- `SecurityHeaders` - Tests implementation of security-related HTTP headers
- `CookieSecurity` - Tests secure cookie handling and attributes
- `RateLimitingConcept` - Tests rate limiting mechanisms and concepts

### 5. PerformanceTest (10 tests)
**File**: `test_performance.cpp` 

Tests performance, concurrency, and scalability aspects of the HTTP server.

#### Concurrency Tests (3 tests)
- `ConcurrentRequestParsing` - Tests parsing 1000 concurrent HTTP requests
- `ConcurrentStatisticsUpdates` - Tests thread-safe statistics updates
- `ThreadPoolStressTest` - Tests thread pool under high concurrent load

#### Memory & Resource Tests (3 tests)
- `MemoryUsageUnderLoad` - Tests memory usage patterns under sustained load
- `MemoryFragmentation` - Tests memory allocation patterns and fragmentation
- `LargeResponseGeneration` - Tests generation of large HTTP responses

#### Configuration & Error Handling Tests (2 tests)
- `RapidConfigurationUpdates` - Tests rapid configuration changes
- `MalformedHttpParsing` - Tests performance of parsing malformed HTTP data

#### Stress & Timeout Tests (2 tests)
- `BinaryDataHandling` - Tests handling of large binary data in requests
- `TimeoutSimulation` - Tests timeout handling and resource cleanup

### 6. HttpProtocolTest (16 tests)
**File**: `test_http_protocol.cpp` 

Tests advanced HTTP/1.1 protocol compliance including chunked encoding, compression, and protocol-specific features. All chunked transfer encoding features are fully implemented and tested.

#### Chunked Transfer Encoding Tests (4 tests)
- `ChunkedEncodingParsing` - Tests parsing of chunked transfer encoding
- `ChunkedEncodingWithEmptyChunks` - Tests handling of empty chunks
- `ChunkedEncodingWithExtensions` - Tests chunk extensions support
- `ChunkedEncodingResponse` - Tests generation of chunked responses

#### Compression & Encoding Tests (3 tests)
- `GzipCompressionSupport` - Tests gzip compression and decompression utilities
- `ContentEncodingHeaders` - Tests Content-Encoding header handling
- `AcceptEncodingProcessing` - Tests Accept-Encoding header parsing

#### HTTP/1.1 Protocol Features Tests (4 tests)
- `PersistentConnections` - Tests HTTP/1.1 keep-alive connection handling
- `TransferEncodingPriority` - Tests Transfer-Encoding vs Content-Length priority
- `MultipleTransferEncodings` - Tests multiple transfer encoding values
- `HostHeaderRequired` - Tests HTTP/1.1 Host header requirement validation

#### Advanced Protocol Tests (5 tests)
- `UpgradeHeader` - Tests protocol upgrade mechanisms (WebSocket handshake)
- `ExpectContinue` - Tests Expect: 100-continue header support
- `RangeRequests` - Tests HTTP range request and partial content support
- `TrailerHeaders` - Tests trailer headers with chunked encoding
- `HttpVersionValidation` - Tests HTTP version validation and compatibility

**Note**: All tests are currently passing.

### 7. HttpsServerTest (11 tests)
**File**: `test_https.cpp` 

Tests HTTPS/SSL functionality including SSL context initialization, certificate handling, and encrypted connections.

#### HTTPS Configuration Tests (4 tests)
- `HttpsConfigurationParsing` - Tests parsing of HTTPS-specific configuration from JSON
- `HttpsConfigurationSerialization` - Tests serialization of HTTPS configuration to JSON
- `MixedHttpHttpsConfiguration` - Tests mixed HTTP and HTTPS server configuration
- `HttpsDisabledConfiguration` - Tests server behavior when HTTPS is disabled

#### SSL/TLS Infrastructure Tests (3 tests)
- `HttpsServerInitialization` - Tests HTTPS server initialization with SSL context
- `SslContextValidation` - Tests SSL context creation and certificate loading
- `SslConnectionBasics` - Tests basic SSL connection handling and lifecycle

#### HTTPS Features Tests (4 tests)
- `HttpsStatisticsTracking` - Tests statistics collection for HTTPS connections
- `HttpsConfigFileLoading` - Tests loading HTTPS configuration from external files
- `HttpsRoutingBasics` - Tests HTTP routing functionality over SSL connections
- `SslCipherConfiguration` - Tests SSL cipher suite configuration and validation

**Dependencies**: Requires OpenSSL for SSL/TLS support and test certificates for validation.

### 8. WebSocketTest (17 tests)
**File**: `test_websocket.cpp` 

Tests comprehensive WebSocket functionality including RFC 6455 protocol compliance, frame handling, connection management, and real-time communication features.

#### Frame Processing Tests (4 tests)
- `FrameSerializationAndParsing` - Tests WebSocket frame serialization and parsing with various opcodes
- `BinaryFrameHandling` - Tests handling of binary WebSocket frames and data integrity
- `MaskedFrames` - Tests client-side frame masking and unmasking according to RFC 6455
- `ControlFrames` - Tests PING, PONG, and CLOSE control frame handling

#### Protocol Compliance Tests (4 tests)
- `KeyGeneration` - Tests WebSocket key generation and Sec-WebSocket-Accept computation
- `RequestValidation` - Tests WebSocket handshake request validation and header checking
- `InvalidRequestHandling` - Tests rejection of invalid WebSocket upgrade requests
- `HandshakeResponseGeneration` - Tests proper WebSocket handshake response generation

#### Server Integration Tests (3 tests)
- `RouteRegistration` - Tests WebSocket route registration in HTTP server
- `ServerStatistics` - Tests WebSocket connection statistics tracking
- `ConnectionStateManagement` - Tests WebSocket connection lifecycle and state transitions

#### Performance & Error Handling Tests (3 tests)
- `FrameSizeLimits` - Tests enforcement of WebSocket frame size limits
- `FrameParsingErrors` - Tests error handling for malformed WebSocket frames
- `FramePerformance` - Tests WebSocket frame processing performance under load

#### Advanced Features Tests (3 tests)
- `ProtocolCompliance` - Tests comprehensive RFC 6455 protocol compliance
- `ConcurrentOperations` - Tests thread-safe WebSocket operations and concurrent connections
- `ConnectionCleanup` - Tests proper WebSocket connection cleanup and resource management

**Dependencies**: Requires Boost.Asio for async I/O and OpenSSL for SHA1 hashing in handshake processing.

### 9. RateLimiterTest (13 tests)
**File**: `test_rate_limiter.cpp` 

Tests comprehensive rate limiting functionality including multiple algorithms, key extraction strategies, middleware integration, and protection against abuse.

#### Algorithm Implementation Tests (5 tests)
- `TokenBucketAllowsBurstRequests` - Tests token bucket algorithm allowing burst capacity requests immediately
- `TokenBucketRefillsOverTime` - Tests token bucket refill mechanism over time intervals
- `FixedWindowEnforcesLimit` - Tests fixed window algorithm enforcing request limits per time window
- `FixedWindowResetsAfterDuration` - Tests fixed window reset behavior after time window expires
- `SlidingWindowEnforcesLimit` - Tests sliding window algorithm with timestamp-based tracking

#### Multi-Client & Key Extraction Tests (3 tests)
- `DifferentClientsHaveSeparateLimits` - Tests that different clients (by IP) have separate rate limits
- `CustomKeyExtractor` - Tests custom key extraction functions for user-based limiting
- `KeyExtractors` - Tests various built-in key extractors (IP, API key, user ID, endpoint path)

#### Configuration & Middleware Tests (3 tests)
- `DisabledLimiterAllowsAllRequests` - Tests that disabled rate limiter allows unlimited requests
- `ConfigurationUpdate` - Tests runtime configuration updates and algorithm switching
- `MiddlewareIntegration` - Tests rate limiting middleware integration with HTTP pipeline

#### Advanced Features Tests (2 tests)
- `ConcurrentAccess` - Tests thread-safe concurrent access to rate limiting algorithms
- `CleanupExpiredEntries` - Tests automatic cleanup of expired rate limiting entries

**Features Tested**:
- **Token Bucket**: Burst handling, refill rates, capacity management
- **Fixed Window**: Request counting, window resets, limit enforcement  
- **Sliding Window**: Timestamp tracking, rolling window behavior
- **Key Strategies**: IP-based, user-based, API key-based, endpoint-based limiting
- **Middleware**: HTTP 429 responses, rate limit headers, custom responses
- **Configuration**: JSON config, runtime updates, algorithm switching
- **Concurrency**: Thread-safe operations, concurrent client handling
- **Cleanup**: Automatic expired entry removal, memory management

### 10. ETagTest (11 tests)
**File**: `test_etag.cpp` 

Tests ETag generation, conditional request handling, and HTTP cache validation functionality.

#### ETag Generation Tests (3 tests)
- `GenerateETag` - Tests ETag generation from content strings with consistent hashing
- `GenerateFileETag` - Tests file-based ETag generation using file metadata and modification times
- `SetAndGetETag` - Tests ETag header setting and retrieval with strong and weak ETag formats

#### Conditional Request Tests (3 tests)
- `ConditionalRequestHeaders` - Tests If-None-Match and If-Modified-Since header parsing and detection
- `ETagMatching` - Tests ETag comparison logic including weak ETags, multiple ETags, and wildcard matching
- `LastModified` - Tests Last-Modified header setting, formatting, and time handling

#### Cache Validation Tests (3 tests)
- `ConditionalFileResponse_NotModified_ETag` - Tests HTTP 304 Not Modified responses for unchanged files using ETags
- `ConditionalFileResponse_NotModified_LastModified` - Tests Last-Modified header infrastructure and conditional request handling
- `ConditionalFileResponse_Modified` - Tests full file responses when content has been modified

#### HTTP Time & Cache Tests (2 tests)
- `HTTPTimeFormatting` - Tests HTTP date/time formatting for cache headers
- `CacheHeaders` - Tests cache control headers including public caching and max-age directives

**Features Tested**:
- **ETag Generation**: Content-based and file-based ETag creation with consistent hashing
- **Conditional Requests**: If-None-Match header processing and HTTP 304 responses
- **Cache Headers**: Last-Modified, Cache-Control, and ETag header management
- **File Modification Detection**: File timestamp tracking and modification detection
- **HTTP Time Formatting**: RFC 1123 date formatting for cache-related headers
- **Weak vs Strong ETags**: Proper handling of both ETag validation types


## Test Data and Fixtures

The tests use various mock data and fixtures:

- **Sample HTTP Requests**: GET, POST, PUT, DELETE requests with different headers and bodies
- **WebSocket Frames**: Various frame types including text, binary, control frames with different sizes and masking
- **WebSocket Handshakes**: Valid and invalid WebSocket upgrade requests and response validation
- **Rate Limiting Scenarios**: Multiple client IPs, user IDs, API keys, different algorithms and configurations
- **HTTPS Test Certificates**: Self-signed certificates for SSL/TLS testing
- **Configuration Files**: Valid and invalid JSON configuration samples including HTTPS settings and rate limiting policies
- **Static Files**: Mock file content for testing file serving functionality
- **Error Scenarios**: Malformed requests, invalid configurations, network errors, SSL handshake failures, invalid WebSocket frames, rate limit violations

## Dependencies

- **Google Test**: Unit testing framework (automatically fetched by CMake)
- **nlohmann/json**: JSON parsing and serialization  
- **Boost.Asio**: Networking and I/O operations
- **OpenSSL**: SSL/TLS support for HTTPS functionality
- **ZLIB**: Compression support for testing
- **C++20**: Modern C++ features used throughout the codebase

All dependencies except the C++ compiler are automatically fetched and configured by CMake.

**HTTPS Testing Requirements**: HTTPS tests require OpenSSL to be installed on the system. Test certificates are automatically generated in the `certs/` directory for development and testing purposes.

## Coverage Areas

The test suite validates:

**HTTP Protocol Compliance**: Request/response parsing and generation  
**HTTPS/SSL Support**: Certificate handling, SSL context management, encrypted connections  
**WebSocket Implementation**: RFC 6455 compliance, frame handling, connection management, real-time communication  
**Rate Limiting**: Multiple algorithms (Token Bucket, Fixed Window, Sliding Window), key extraction, middleware integration  
**Configuration Management**: JSON config loading, validation, and updates  
**Concurrency**: Thread pool operations and thread safety  
**Error Handling**: Graceful handling of invalid inputs and edge cases  
**File Operations**: Static file serving and MIME type detection  
**Performance**: Large data handling and resource management  
**Standards Compliance**: HTTP headers, status codes, and method handling  

## Adding New Tests

When adding new test cases:

1. Follow the existing naming convention: `TestSuite.TestName`
2. Use descriptive test names that explain what is being validated
3. Include both positive and negative test cases
4. Test edge cases and error conditions
5. Keep tests isolated and independent
6. Use appropriate Google Test assertions (`EXPECT_*`, `ASSERT_*`)

## Continuous Integration

These tests are automatically executed in the CI/CD pipeline on:
- Every pull request
- Every push to main branch
- Nightly builds across multiple platforms (Ubuntu, macOS)
- Multiple compiler configurations (GCC, Clang)


---
