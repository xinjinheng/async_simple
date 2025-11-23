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
#ifndef ASYNC_SIMPLE_NETWORK_OPERATION_WRAPPER_H
#define ASYNC_SIMPLE_NETWORK_OPERATION_WRAPPER_H

#include <async_simple/coro/Lazy.h>
#include <async_simple/NetworkException.h>
#include <asio/io_context.hpp>
#include <asio/steady_timer.hpp>
#include <asio/error_code.hpp>
#include <variant>

namespace async_simple {
namespace coro {

// Network operation wrapper with timeout support
template <typename ResultType>
class NetworkOperationWrapper {
public:
    NetworkOperationWrapper(asio::io_context& ioContext, 
                           const NetworkTimeoutConfig& timeoutConfig = {})
        : _ioContext(ioContext), 
          _timeoutConfig(timeoutConfig),
          _timer(ioContext) {}
    
    // Start the network operation with timeout
    template <typename Op>
    Lazy<ResultType> start(Op&& op) {
        if (!_timeoutConfig.enableTimeout || _timeoutConfig.timeoutMs == 0) {
            // No timeout, just execute the operation
            co_return co_await std::forward<Op>(op)();
        }
        
        // Set up timeout timer
        _timer.expires_after(std::chrono::milliseconds(_timeoutConfig.timeoutMs));
        
        // Create a promise to hold the result
        auto promise = std::make_shared<Promise<ResultType>>();
        auto future = promise->getFuture();
        
        // Start the network operation
        std::forward<Op>(op)([promise, this](const asio::error_code& ec, ResultType result) {
            if (ec) {
                if (ec == asio::error::operation_aborted) {
                    // Operation was aborted due to timeout
                    promise->setException(std::make_exception_ptr(
                        ConnectionTimeoutException("Network operation timed out")));
                }
                else {
                    // Convert asio error to exception
                    auto networkEx = convertAsioErrorToException(ec, "Network operation");
                    promise->setException(std::make_exception_ptr(*networkEx));
                }
            }
            else {
                promise->setValue(std::move(result));
            }
            
            // Cancel the timer if it's still running
            asio::error_code timerEc;
            _timer.cancel(timerEc);
        });
        
        // Start the timeout timer
        _timer.async_wait([promise, this](const asio::error_code& ec) {
            if (!ec) {
                // Timeout occurred
                promise->setException(std::make_exception_ptr(
                    ConnectionTimeoutException("Network operation timed out")));
                
                // Cancel the network operation if possible
                if (_cancelOp) {
                    _cancelOp();
                }
            }
        });
        
        // Wait for the result or timeout
        try {
            co_return co_await std::move(future);
        }
        catch (const ConnectionTimeoutException&) {
            // Rethrow timeout exception
            throw;
        }
        catch (const NetworkException&) {
            // Rethrow network exception
            throw;
        }
        catch (const std::exception& ex) {
            // Wrap other exceptions
            throw NetworkException("Unexpected error in network operation: " + std::string(ex.what()));
        }
    }
    
    // Set cancel operation callback
    void setCancelOp(std::function<void()> cancelOp) {
        _cancelOp = std::move(cancelOp);
    }
    
private:
    asio::io_context& _ioContext;
    NetworkTimeoutConfig _timeoutConfig;
    asio::steady_timer _timer;
    std::function<void()> _cancelOp;
};

// Specialization for void result type
template <>
class NetworkOperationWrapper<void> {
public:
    NetworkOperationWrapper(asio::io_context& ioContext, 
                           const NetworkTimeoutConfig& timeoutConfig = {})
        : _ioContext(ioContext), 
          _timeoutConfig(timeoutConfig),
          _timer(ioContext) {}
    
    // Start the network operation with timeout
    template <typename Op>
    Lazy<void> start(Op&& op) {
        if (!_timeoutConfig.enableTimeout || _timeoutConfig.timeoutMs == 0) {
            // No timeout, just execute the operation
            co_await std::forward<Op>(op)();
            co_return;
        }
        
        // Set up timeout timer
        _timer.expires_after(std::chrono::milliseconds(_timeoutConfig.timeoutMs));
        
        // Create a promise to hold the result
        auto promise = std::make_shared<Promise<void>>();
        auto future = promise->getFuture();
        
        // Start the network operation
        std::forward<Op>(op)([promise, this](const asio::error_code& ec) {
            if (ec) {
                if (ec == asio::error::operation_aborted) {
                    // Operation was aborted due to timeout
                    promise->setException(std::make_exception_ptr(
                        ConnectionTimeoutException("Network operation timed out")));
                }
                else {
                    // Convert asio error to exception
                    auto networkEx = convertAsioErrorToException(ec, "Network operation");
                    promise->setException(std::make_exception_ptr(*networkEx));
                }
            }
            else {
                promise->setValue();
            }
            
            // Cancel the timer if it's still running
            asio::error_code timerEc;
            _timer.cancel(timerEc);
        });
        
        // Start the timeout timer
        _timer.async_wait([promise, this](const asio::error_code& ec) {
            if (!ec) {
                // Timeout occurred
                promise->setException(std::make_exception_ptr(
                    ConnectionTimeoutException("Network operation timed out")));
                
                // Cancel the network operation if possible
                if (_cancelOp) {
                    _cancelOp();
                }
            }
        });
        
        // Wait for the result or timeout
        try {
            co_await std::move(future);
        }
        catch (const ConnectionTimeoutException&) {
            // Rethrow timeout exception
            throw;
        }
        catch (const NetworkException&) {
            // Rethrow network exception
            throw;
        }
        catch (const std::exception& ex) {
            // Wrap other exceptions
            throw NetworkException("Unexpected error in network operation: " + std::string(ex.what()));
        }
    }
    
    // Set cancel operation callback
    void setCancelOp(std::function<void()> cancelOp) {
        _cancelOp = std::move(cancelOp);
    }
    
private:
    asio::io_context& _ioContext;
    NetworkTimeoutConfig _timeoutConfig;
    asio::steady_timer _timer;
    std::function<void()> _cancelOp;
};

}  // namespace coro
}  // namespace async_simple

#endif  // ASYNC_SIMPLE_NETWORK_OPERATION_WRAPPER_H
