/**
 * @file connection.cpp
 * @brief Implementation of the Connection class for managing individual HTTP client connections.
 *
 * Handles asynchronous reading, writing, timeouts, and request/response processing for each client connection.
 */
#include "connection.hpp"
#include <iostream>

namespace http_server {

Connection::Connection(boost::asio::ip::tcp::socket socket, RequestHandler handler, 
                       std::function<void()> cleanup_callback)
    : socket_(std::move(socket))
    , request_handler_(std::move(handler))
    , cleanup_callback_(std::move(cleanup_callback))
    , creation_time_(std::chrono::steady_clock::now())
    , timeout_timer_(socket_.get_executor()) {
}

Connection::~Connection() {
    if (cleanup_callback_) {
        cleanup_callback_();
    }
}

void Connection::start() {
    setup_timeout();
    read_request();
}

std::string Connection::client_address() const {
    try {
        return socket_.remote_endpoint().address().to_string();
    } catch (const std::exception&) {
        return "unknown";
    }
}

std::string Connection::client_port() const {
    try {
        return std::to_string(socket_.remote_endpoint().port());
    } catch (const std::exception&) {
        return "0";
    }
}

bool Connection::is_open() const {
    return socket_.is_open();
}

void Connection::close() {
    timeout_timer_.cancel();
    
    if (socket_.is_open()) {
        boost::system::error_code ec;
        socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
        socket_.close(ec);
    }
}

void Connection::read_request() {
    auto self = shared_from_this();
    socket_.async_read_some(
        boost::asio::buffer(buffer_),
        [self](const boost::system::error_code& error, size_t bytes_transferred) {
            self->handle_read(error, bytes_transferred);
        }
    );
}

void Connection::handle_read(const boost::system::error_code& error, size_t bytes_transferred) {
    if (error) {
        handle_error(error);
        return;
    }
    
    bytes_received_ += bytes_transferred;
    request_data_.append(buffer_.data(), bytes_transferred);
    
    if (request_data_.size() > MAX_REQUEST_SIZE) {
        auto response = HttpResponse(HttpStatus::PAYLOAD_TOO_LARGE);
        response.set_text("Request entity too large");
        send_response(response);
        return;
    }
    
    if (is_request_complete()) {
        timeout_timer_.cancel();
        process_request();
    } else {
        read_request();
    }
}

void Connection::process_request() {
    try {
        auto request_opt = HttpRequest::parse(request_data_);
        
        if (!request_opt) {
            auto response = HttpResponse(HttpStatus::BAD_REQUEST);
            response.set_text("Invalid HTTP request");
            send_response(response);
            return;
        }
        
        auto request = *request_opt;
        auto response = request_handler_(request);
        
        if (request.is_keep_alive() && response.get_header("Connection").empty()) {
            response.set_keep_alive(true);
        }
        
        send_response(response);
        
    } catch (const std::exception& e) {
        auto response = HttpResponse(HttpStatus::INTERNAL_SERVER_ERROR);
        response.set_text("Internal server error: " + std::string(e.what()));
        send_response(response);
    }
}

void Connection::send_response(const HttpResponse& response) {
    auto self = shared_from_this();
    auto response_headers = std::make_shared<std::string>(response.to_http_string());
    bytes_sent_ += response_headers->size();

    boost::asio::async_write(
        socket_,
        boost::asio::buffer(*response_headers),
        [self, response_headers, response](const boost::system::error_code& error, size_t bytes_transferred) {
            if (!error) {
                auto body_stream = response.body_stream();
                if (body_stream) {
                    self->write_body_chunk(response, body_stream);
                } else {
                    self->handle_write(error, bytes_transferred, nullptr);
                }
            } else {
                self->handle_error(error);
            }
        }
    );
}

void Connection::write_body_chunk(const HttpResponse& response, std::shared_ptr<std::istream> body_stream) {
    auto self = shared_from_this();
    auto chunk_buffer = std::make_shared<std::vector<char>>(8192);

    body_stream->read(chunk_buffer->data(), chunk_buffer->size());
    std::streamsize bytes_read = body_stream->gcount();

    if (bytes_read > 0) {
        bytes_sent_ += bytes_read;
        boost::asio::async_write(
            socket_,
            boost::asio::buffer(chunk_buffer->data(), bytes_read),
            [self, response, body_stream, chunk_buffer](const boost::system::error_code& error, size_t /*bytes_transferred*/) {
                if (!error) {
                    self->write_body_chunk(response, body_stream);
                } else {
                    self->handle_error(error);
                }
            }
        );
    } else {
        // End of body stream
        handle_write(boost::system::error_code(), 0, nullptr);
    }
}

void Connection::handle_write(const boost::system::error_code& error, size_t /*bytes_transferred*/,
                             std::shared_ptr<std::string> /*response_data*/) {
    if (error) {
        handle_error(error);
        return;
    }
    
    bool keep_alive = false;
    try {
        auto request_opt = HttpRequest::parse(request_data_);
        if (request_opt && request_opt->is_keep_alive()) {
            keep_alive = true;
        }
    } catch (...) {
        // If we can't parse the request, don't keep alive
    }
    
    if (keep_alive) {
        request_data_.clear();
        bytes_received_ = 0;
        bytes_sent_ = 0;
        setup_timeout();
        read_request();
    } else {
        close();
    }
}

bool Connection::is_request_complete() const {
    // Find the end of the headers first
    size_t header_end_pos = request_data_.find("\r\n\r\n");
    if (header_end_pos == std::string::npos) {
        header_end_pos = request_data_.find("\n\n");
        if (header_end_pos == std::string::npos) {
            return false; // Headers not fully received yet
        }
    }

    // Temporarily parse headers to check for Content-Length or Transfer-Encoding
    auto temp_request = HttpRequest::parse(request_data_.substr(0, header_end_pos));
    if (!temp_request) {
        // This could be a malformed request, but we might not have all of it yet.
        // Let more data arrive unless it exceeds max size.
        return false;
    }

    auto transfer_encoding = temp_request->get_header("transfer-encoding");
    if (transfer_encoding && transfer_encoding->find("chunked") != std::string::npos) {
        // For chunked encoding, the request is complete when we see the final chunk "0\r\n\r\n"
        return request_data_.find("\r\n0\r\n\r\n") != std::string::npos || request_data_.find("\n0\r\n\r\n") != std::string::npos;
    } else {
        // For Content-Length, check if we have received the full body
        size_t content_length = temp_request->content_length();
        size_t body_start_pos = request_data_.find("\r\n\r\n") != std::string::npos ? header_end_pos + 4 : header_end_pos + 2;
        size_t current_body_size = request_data_.length() - body_start_pos;
        return current_body_size >= content_length;
    }
}

void Connection::handle_error(const boost::system::error_code& error) {
    if (error != boost::asio::error::operation_aborted &&
        error != boost::asio::error::eof &&
        error != boost::asio::error::connection_reset) {
        std::cerr << "Connection error: " << error.message() << std::endl;
    }
    close();
}

void Connection::setup_timeout() {
    timeout_timer_.expires_after(TIMEOUT);
    
    auto self = shared_from_this();
    timeout_timer_.async_wait(
        [self](const boost::system::error_code& error) {
            self->handle_timeout(error);
        }
    );
}

void Connection::handle_timeout(const boost::system::error_code& error) {
    if (error != boost::asio::error::operation_aborted) {
        std::cerr << "Connection timeout for " << client_address() << std::endl;
        close();
    }
}

} // namespace http_server 