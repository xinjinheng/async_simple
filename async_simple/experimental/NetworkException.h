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
    explicit NetworkException(const char* message)
        : std::runtime_error(message) {}
    virtual ~NetworkException() = default;
};

// Exception for connection timeout
class ConnectionTimeoutException : public NetworkException {
public:
    explicit ConnectionTimeoutException(const std::string& message)
        : NetworkException(message) {}
    explicit ConnectionTimeoutException(const char* message)
        : NetworkException(message) {}
};

// Exception for read/write failure
class IoException : public NetworkException {
public:
    explicit IoException(const std::string& message)
        : NetworkException(message) {}
    explicit IoException(const char* message)
        : NetworkException(message) {}
    explicit IoException(const std::error_code& ec)
        : NetworkException("IO error: " + ec.message()) {}
};

// Exception for protocol error
class ProtocolException : public NetworkException {
public:
    explicit ProtocolException(const std::string& message)
        : NetworkException(message) {}
    explicit ProtocolException(const char* message)
        : NetworkException(message) {}
};

// Exception for connection closed by peer
class ConnectionClosedException : public NetworkException {
public:
    explicit ConnectionClosedException(const std::string& message)
        : NetworkException(message) {}
    explicit ConnectionClosedException(const char* message)
        : NetworkException(message) {}
};

// Exception for address resolution failure
class AddressResolutionException : public NetworkException {
public:
    explicit AddressResolutionException(const std::string& message)
        : NetworkException(message) {}
    explicit AddressResolutionException(const char* message)
        : NetworkException(message) {}
    explicit AddressResolutionException(const std::error_code& ec)
        : NetworkException("Address resolution error: " + ec.message()) {}
};

}  // namespace experimental
}  // namespace async_simple

#endif  // ASYNC_SIMPLE_EXPERIMENTAL_NETWORK_EXCEPTION_H
