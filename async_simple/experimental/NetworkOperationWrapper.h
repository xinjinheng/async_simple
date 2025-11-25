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
#ifndef ASYNC_SIMPLE_EXPERIMENTAL_NETWORK_OPERATION_WRAPPER_H
#define ASYNC_SIMPLE_EXPERIMENTAL_NETWORK_OPERATION_WRAPPER_H

#include <chrono>
#include <memory>
#include <variant>
#include <asio.hpp>
#include "async_simple/coro/Lazy.h"
#include "async_simple/experimental/NetworkException.h"

namespace async_simple {
namespace experimental {

// Default network operation timeout
constexpr std::chrono::seconds DEFAULT_NETWORK_TIMEOUT = std::chrono::seconds(30);

// Network operation wrapper with timeout support
template <typename ResultType>
class NetworkOperationWrapper {
public:
    template <typename Op, typename... Args>
    static coro::Lazy<ResultType> wrap(
        asio::io_context& io_context,
        Op&& op,
        Args&&... args,
        std::chrono::milliseconds timeout = std::chrono::duration_cast<std::chrono::milliseconds>(DEFAULT_NETWORK_TIMEOUT)) {
        // Create a promise and future to synchronize the operation
        auto promise = std::make_shared<std::promise<ResultType>>();
        auto future = promise->get_future();

        // Create a timeout timer
        asio::steady_timer timer(io_context, timeout);

        // Flag to indicate if the operation has completed
        auto completed = std::make_shared<std::atomic<bool>>(false);

        // Start the timeout timer
        timer.async_wait([promise, completed](const std::error_code& ec) {
            if (ec == asio::error::operation_aborted) {
                return; // Timer was canceled
            }
            if (!completed->exchange(true)) {
                try {
                    throw ConnectionTimeoutException("Network operation timed out");
                } catch (...) {
                    promise->set_exception(std::current_exception());
                }
            }
        });

        // Start the actual network operation
        std::forward<Op>(op)(std::forward<Args>(args)..., [promise, completed, &timer](const std::error_code& ec, auto&&... results) {
            if (!completed->exchange(true)) {
                // Cancel the timeout timer
                std::error_code timer_ec;
                timer.cancel(timer_ec);

                if (ec) {
                    // Convert asio error code to exception
                    if (ec == asio::error::operation_aborted) {
                        promise->set_exception(std::make_exception_ptr(ConnectionClosedException("Network operation aborted")));
                    } else if (ec == asio::error::connection_reset || ec == asio::error::connection_aborted) {
                        promise->set_exception(std::make_exception_ptr(ConnectionClosedException("Connection closed by peer")));
                    } else {
                        promise->set_exception(std::make_exception_ptr(IoException(ec)));
                    }
                } else {
                    // Set the result
                    if constexpr (std::is_void_v<ResultType>) {
                        promise->set_value();
                    } else if constexpr (std::tuple_size_v<std::decay_t<ResultType>> == 1) {
                        promise->set_value(std::make_tuple(std::forward<decltype(results)>(results)...));
                    } else {
                        promise->set_value(std::forward<decltype(results)>(results)...);
                    }
                }
            }
        });

        // Wait for the result or timeout
        try {
            if constexpr (std::is_void_v<ResultType>) {
                future.get();
                co_return;
            } else {
                co_return future.get();
            }
        } catch (const std::exception& e) {
            // Re-throw the exception
            throw;
        }
    }
};

// Specialization for void result type
template <>
class NetworkOperationWrapper<void> {
public:
    template <typename Op, typename... Args>
    static coro::Lazy<void> wrap(
        asio::io_context& io_context,
        Op&& op,
        Args&&... args,
        std::chrono::milliseconds timeout = std::chrono::duration_cast<std::chrono::milliseconds>(DEFAULT_NETWORK_TIMEOUT)) {
        // Create a promise and future to synchronize the operation
        auto promise = std::make_shared<std::promise<void>>();
        auto future = promise->get_future();

        // Create a timeout timer
        asio::steady_timer timer(io_context, timeout);

        // Flag to indicate if the operation has completed
        auto completed = std::make_shared<std::atomic<bool>>(false);

        // Start the timeout timer
        timer.async_wait([promise, completed](const std::error_code& ec) {
            if (ec == asio::error::operation_aborted) {
                return; // Timer was canceled
            }
            if (!completed->exchange(true)) {
                try {
                    throw ConnectionTimeoutException("Network operation timed out");
                } catch (...) {
                    promise->set_exception(std::current_exception());
                }
            }
        });

        // Start the actual network operation
        std::forward<Op>(op)(std::forward<Args>(args)..., [promise, completed, &timer](const std::error_code& ec) {
            if (!completed->exchange(true)) {
                // Cancel the timeout timer
                std::error_code timer_ec;
                timer.cancel(timer_ec);

                if (ec) {
                    // Convert asio error code to exception
                    if (ec == asio::error::operation_aborted) {
                        promise->set_exception(std::make_exception_ptr(ConnectionClosedException("Network operation aborted")));
                    } else if (ec == asio::error::connection_reset || ec == asio::error::connection_aborted) {
                        promise->set_exception(std::make_exception_ptr(ConnectionClosedException("Connection closed by peer")));
                    } else {
                        promise->set_exception(std::make_exception_ptr(IoException(ec)));
                    }
                } else {
                    // Set the result
                    promise->set_value();
                }
            }
        });

        // Wait for the result or timeout
        try {
            future.get();
            co_return;
        } catch (const std::exception& e) {
            // Re-throw the exception
            throw;
        }
    }
};

// Helper functions for common network operations
inline coro::Lazy<std::pair<std::error_code, asio::ip::tcp::socket>> async_connect(
    asio::io_context& io_context,
    const std::string& host,
    const std::string& port,
    std::chrono::milliseconds timeout = std::chrono::duration_cast<std::chrono::milliseconds>(DEFAULT_NETWORK_TIMEOUT)) {
    asio::ip::tcp::socket socket(io_context);
    asio::ip::tcp::resolver resolver(io_context);

    auto endpoints = co_await NetworkOperationWrapper<std::vector<asio::ip::tcp::endpoint>>::wrap(
        io_context,
        [&](auto&& handler) {
            resolver.async_resolve(host, port, std::forward<decltype(handler)>(handler));
        },
        timeout);

    co_await NetworkOperationWrapper<void>::wrap(
        io_context,
        [&](auto&& handler) {
            asio::async_connect(socket, endpoints, std::forward<decltype(handler)>(handler));
        },
        timeout);

    co_return std::make_pair(std::error_code(), std::move(socket));
}

inline coro::Lazy<std::pair<std::error_code, size_t>> async_read_some(
    asio::ip::tcp::socket& socket,
    asio::mutable_buffer buffer,
    std::chrono::milliseconds timeout = std::chrono::duration_cast<std::chrono::milliseconds>(DEFAULT_NETWORK_TIMEOUT)) {
    auto result = co_await NetworkOperationWrapper<std::tuple<size_t>>::wrap(
        socket.get_executor().context(),
        [&](auto&& handler) {
            socket.async_read_some(buffer, std::forward<decltype(handler)>(handler));
        },
        timeout);

    co_return std::make_pair(std::error_code(), std::get<0>(result));
}

inline coro::Lazy<std::pair<std::error_code, size_t>> async_write(
    asio::ip::tcp::socket& socket,
    asio::const_buffer buffer,
    std::chrono::milliseconds timeout = std::chrono::duration_cast<std::chrono::milliseconds>(DEFAULT_NETWORK_TIMEOUT)) {
    auto result = co_await NetworkOperationWrapper<std::tuple<size_t>>::wrap(
        socket.get_executor().context(),
        [&](auto&& handler) {
            asio::async_write(socket, buffer, std::forward<decltype(handler)>(handler));
        },
        timeout);

    co_return std::make_pair(std::error_code(), std::get<0>(result));
}

inline coro::Lazy<std::pair<std::error_code, asio::ip::tcp::socket>> async_accept(
    asio::ip::tcp::acceptor& acceptor,
    std::chrono::milliseconds timeout = std::chrono::duration_cast<std::chrono::milliseconds>(DEFAULT_NETWORK_TIMEOUT)) {
    asio::ip::tcp::socket socket(acceptor.get_executor().context());

    co_await NetworkOperationWrapper<void>::wrap(
        acceptor.get_executor().context(),
        [&](auto&& handler) {
            acceptor.async_accept(socket, std::forward<decltype(handler)>(handler));
        },
        timeout);

    co_return std::make_pair(std::error_code(), std::move(socket));
}

}  // namespace experimental
}  // namespace async_simple

#endif  // ASYNC_SIMPLE_EXPERIMENTAL_NETWORK_OPERATION_WRAPPER_H
