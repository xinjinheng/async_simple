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
#ifndef ASYNC_SIMPLE_EXPERIMENTAL_NETWORK_EXCEPTION_H
#define ASYNC_SIMPLE_EXPERIMENTAL_NETWORK_EXCEPTION_H

#include <stdexcept>
#include <string>
#include <system_error>

namespace async_simple {
namespace experimental {

// Base class for all network exceptions
class NetworkException : public std::runtime_error {
public:
    explicit NetworkException(const std::string& message)
        : std::runtime_error(message) {}
    
    explicit NetworkException(const std::error_code& ec)
        : std::runtime_error(ec.message()), _errorCode(ec) {}
    
    const std::error_code& errorCode() const noexcept {
        return _errorCode;
    }
    
private:
    std::error_code _errorCode;
};

// Connection timeout exception
class ConnectionTimeoutException : public NetworkException {
public:
    explicit ConnectionTimeoutException(const std::string& message)
        : NetworkException(message) {}
    
    explicit ConnectionTimeoutException(const std::error_code& ec)
        : NetworkException(ec) {}
};

// Read timeout exception
class ReadTimeoutException : public NetworkException {
public:
    explicit ReadTimeoutException(const std::string& message)
        : NetworkException(message) {}
    
    explicit ReadTimeoutException(const std::error_code& ec)
        : NetworkException(ec) {}
};

// Write timeout exception
class WriteTimeoutException : public NetworkException {
public:
    explicit WriteTimeoutException(const std::string& message)
        : NetworkException(message) {}
    
    explicit WriteTimeoutException(const std::error_code& ec)
        : NetworkException(ec) {}
};

// Connection closed exception
class ConnectionClosedException : public NetworkException {
public:
    explicit ConnectionClosedException(const std::string& message)
        : NetworkException(message) {}
    
    explicit ConnectionClosedException(const std::error_code& ec)
        : NetworkException(ec) {}
};

// Protocol error exception
class ProtocolErrorException : public NetworkException {
public:
    explicit ProtocolErrorException(const std::string& message)
        : NetworkException(message) {}
    
    explicit ProtocolErrorException(const std::error_code& ec)
        : NetworkException(ec) {}
};

// Network operation failed exception
class NetworkOperationFailedException : public NetworkException {
public:
    explicit NetworkOperationFailedException(const std::string& message)
        : NetworkException(message) {}
    
    explicit NetworkOperationFailedException(const std::error_code& ec)
        : NetworkException(ec) {}
};

}  // namespace experimental
}  // namespace async_simple

#endif  // ASYNC_SIMPLE_EXPERIMENTAL_NETWORK_EXCEPTION_H
