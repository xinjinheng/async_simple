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
#ifndef ASYNC_SIMPLE_EXPERIMENTAL_CONNECTION_POOL_H
#define ASYNC_SIMPLE_EXPERIMENTAL_CONNECTION_POOL_H

#ifndef ASYNC_SIMPLE_USE_MODULES
#include <atomic>
#include <chrono>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "async_simple/Executor.h"
#include "async_simple/coro/Lazy.h"
#include "async_simple/coro/Timeout.h"

#endif  // ASYNC_SIMPLE_USE_MODULES

namespace async_simple {
namespace experimental {

// Connection interface that all connection types must implement
template <typename ConnectionType>
concept Connection = requires(ConnectionType& conn) {
    { conn.isValid() } -> std::convertible_to<bool>;
    { conn.healthCheck() } -> std::convertible_to<bool>;
    { conn.close() } -> std::convertible_to<void>;
};

// Connection pool configuration
struct ConnectionPoolConfig {
    size_t max_connections = 100;
    size_t min_connections = 10;
    std::chrono::milliseconds connection_timeout = std::chrono::seconds(30);
    std::chrono::milliseconds health_check_interval = std::chrono::seconds(60);
    std::chrono::milliseconds idle_timeout = std::chrono::minutes(5);
    size_t max_retry_attempts = 3;
    std::chrono::milliseconds retry_delay = std::chrono::milliseconds(100);
};

// Connection pool implementation
template <typename ConnectionType>
requires Connection<ConnectionType>
class ConnectionPool {
public:
    using CreateConnectionFunc = std::function<coro::Lazy<std::shared_ptr<ConnectionType>>()>;
    using ValidateConnectionFunc = std::function<bool(const std::shared_ptr<ConnectionType>&)>;
    using DestroyConnectionFunc = std::function<void(std::shared_ptr<ConnectionType>)>;

    ConnectionPool(CreateConnectionFunc create_func,
                   ValidateConnectionFunc validate_func = nullptr,
                   DestroyConnectionFunc destroy_func = nullptr,
                   ConnectionPoolConfig config = ConnectionPoolConfig())
        : _create_func(std::move(create_func))
        , _validate_func(validate_func ? std::move(validate_func) : [](const std::shared_ptr<ConnectionType>& conn) {
            return conn->isValid();
        })
        , _destroy_func(destroy_func ? std::move(destroy_func) : [](std::shared_ptr<ConnectionType> conn) {
            conn->close();
        })
        , _config(std::move(config))
        , _stop_flag(false)
        , _total_connections(0) {
        
        // Start health check thread
        _health_check_thread = std::thread([this]() {
            healthCheckLoop();
        });
    }

    ~ConnectionPool() {
        _stop_flag = true;
        _cv.notify_all();
        if (_health_check_thread.joinable()) {
            _health_check_thread.join();
        }
        
        // Destroy all connections
        std::unique_lock<std::mutex> lock(_mutex);
        while (!_idle_connections.empty()) {
            auto conn = std::move(_idle_connections.front());
            _idle_connections.pop_front();
            lock.unlock();
            _destroy_func(std::move(conn));
            lock.lock();
        }
        _total_connections = 0;
    }

    // Get a connection from the pool
    coro::Lazy<std::shared_ptr<ConnectionType>> getConnection() {
        auto start_time = std::chrono::steady_clock::now();
        
        for (size_t attempt = 0; attempt < _config.max_retry_attempts; ++attempt) {
            std::shared_ptr<ConnectionType> conn;
            
            {   // Try to get from idle connections
                std::unique_lock<std::mutex> lock(_mutex);
                
                if (!_idle_connections.empty()) {
                    conn = std::move(_idle_connections.front());
                    _idle_connections.pop_front();
                    lock.unlock();
                    
                    // Validate connection
                    if (_validate_func(conn)) {
                        co_return conn;
                    }
                    else {
                        // Connection is invalid, destroy it
                        _destroy_func(std::move(conn));
                        std::unique_lock<std::mutex> lock2(_mutex);
                        --_total_connections;
                    }
                }
                else if (_total_connections < _config.max_connections) {
                    // Create new connection
                    ++_total_connections;
                    lock.unlock();
                    
                    auto create_result = co_await coro::timeout(
                        _create_func(), _config.connection_timeout);
                    
                    if (create_result.hasError()) {
                        std::unique_lock<std::mutex> lock2(_mutex);
                        --_total_connections;
                    }
                    else {
                        co_return create_result.value();
                    }
                }
                else {
                    // Wait for available connection
                    lock.unlock();
                    
                    // Wait with timeout
                    std::this_thread::sleep_for(_config.retry_delay);
                }
            }
            
            // Check if we've exceeded the total timeout
            auto elapsed = std::chrono::steady_clock::now() - start_time;
            if (elapsed > _config.connection_timeout) {
                break;
            }
        }
        
        // Failed to get connection
        co_return nullptr;
    }

    // Return a connection to the pool
    void returnConnection(std::shared_ptr<ConnectionType> conn) {
        if (!conn || !_validate_func(conn)) {
            // Connection is invalid, destroy it
            _destroy_func(std::move(conn));
            std::unique_lock<std::mutex> lock(_mutex);
            --_total_connections;
            return;
        }
        
        std::unique_lock<std::mutex> lock(_mutex);
        _idle_connections.push_back(std::move(conn));
        _cv.notify_one();
    }

    // Get current pool statistics
    struct Stats {
        size_t total_connections;
        size_t idle_connections;
        size_t active_connections;
    };

    Stats getStats() const {
        std::unique_lock<std::mutex> lock(_mutex);
        Stats stats;
        stats.total_connections = _total_connections;
        stats.idle_connections = _idle_connections.size();
        stats.active_connections = stats.total_connections - stats.idle_connections;
        return stats;
    }

private:
    // Health check loop
    void healthCheckLoop() {
        while (!_stop_flag) {
            std::this_thread::sleep_for(_config.health_check_interval);
            
            std::unique_lock<std::mutex> lock(_mutex);
            auto idle_count = _idle_connections.size();
            
            for (size_t i = 0; i < idle_count; ++i) {
                auto conn = std::move(_idle_connections.front());
                _idle_connections.pop_front();
                lock.unlock();
                
                bool valid = false;
                try {
                    valid = conn->healthCheck();
                }
                catch (...) {
                    valid = false;
                }
                
                if (valid) {
                    // Return to pool
                    std::unique_lock<std::mutex> lock2(_mutex);
                    _idle_connections.push_back(std::move(conn));
                }
                else {
                    // Destroy invalid connection
                    _destroy_func(std::move(conn));
                    std::unique_lock<std::mutex> lock2(_mutex);
                    --_total_connections;
                }
                lock.lock();
            }
            
            // Maintain minimum connections
            while (_total_connections < _config.min_connections) {
                ++_total_connections;
                lock.unlock();
                
                auto create_result = coro::syncAwait(
                    coro::timeout(_create_func(), _config.connection_timeout));
                
                if (create_result.hasError()) {
                    std::unique_lock<std::mutex> lock2(_mutex);
                    --_total_connections;
                }
                else {
                    std::unique_lock<std::mutex> lock2(_mutex);
                    _idle_connections.push_back(create_result.value());
                }
                lock.lock();
            }
        }
    }

private:
    CreateConnectionFunc _create_func;
    ValidateConnectionFunc _validate_func;
    DestroyConnectionFunc _destroy_func;
    ConnectionPoolConfig _config;
    
    mutable std::mutex _mutex;
    std::condition_variable _cv;
    std::deque<std::shared_ptr<ConnectionType>> _idle_connections;
    std::atomic<size_t> _total_connections;
    
    std::atomic<bool> _stop_flag;
    std::thread _health_check_thread;
};

}  // namespace experimental
}  // namespace async_simple

#endif