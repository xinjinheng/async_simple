/*
 * Copyright (c) 2022, Alibaba Group Holding Limited;
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef ASYNC_SIMPLE_NETWORK_EXCEPTION_H
#define ASYNC_SIMPLE_NETWORK_EXCEPTION_H

#include <stdexcept>
#include <string>
#include <system_error>
#include <asio/error_code.hpp>

namespace async_simple {

// Base network exception class
class NetworkException : public std::runtime_error {
public:
    explicit NetworkException(const std::string& message)
        : std::runtime_error(message), _errorCode() {}
    
    NetworkException(const std::string& message, const asio::error_code& ec)
        : std::runtime_error(message + ": " + ec.message()), _errorCode(ec) {}
    
    virtual ~NetworkException() = default;
    
    // Get error code
    const asio::error_code& getErrorCode() const noexcept {
        return _errorCode;
    }
    
private:
    asio::error_code _errorCode;
};

// Connection timeout exception
class ConnectionTimeoutException : public NetworkException {
public:
    explicit ConnectionTimeoutException(const std::string& message)
        : NetworkException(message) {}
    
    ConnectionTimeoutException(const std::string& message, const asio::error_code& ec)
        : NetworkException(message, ec) {}
};

// Read/write failure exception
class ReadWriteException : public NetworkException {
public:
    explicit ReadWriteException(const std::string& message)
        : NetworkException(message) {}
    
    ReadWriteException(const std::string& message, const asio::error_code& ec)
        : NetworkException(message, ec) {}
};

// Protocol error exception
class ProtocolException : public NetworkException {
public:
    explicit ProtocolException(const std::string& message)
        : NetworkException(message) {}
    
    ProtocolException(const std::string& message, const asio::error_code& ec)
        : NetworkException(message, ec) {}
};

// Connection closed exception
class ConnectionClosedException : public NetworkException {
public:
    explicit ConnectionClosedException(const std::string& message)
        : NetworkException(message) {}
    
    ConnectionClosedException(const std::string& message, const asio::error_code& ec)
        : NetworkException(message, ec) {}
};

// Network operation wrapper timeout configuration
struct NetworkTimeoutConfig {
    // Global default timeout in milliseconds
    static constexpr uint64_t DEFAULT_TIMEOUT = 30000; // 30 seconds
    
    // Enable/disable timeout for network operations
    bool enableTimeout = true;
    
    // Timeout in milliseconds for this operation
    uint64_t timeoutMs = DEFAULT_TIMEOUT;
    
    // Constructor
    NetworkTimeoutConfig() = default;
    explicit NetworkTimeoutConfig(uint64_t timeoutMs)
        : enableTimeout(true), timeoutMs(timeoutMs) {}
};

// Convert asio error code to NetworkException
std::unique_ptr<NetworkException> convertAsioErrorToException(
    const asio::error_code& ec, const std::string& operation);

}  // namespace async_simple

#endif  // ASYNC_SIMPLE_NETWORK_EXCEPTION_H
