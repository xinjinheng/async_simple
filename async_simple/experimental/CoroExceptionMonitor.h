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
#ifndef ASYNC_SIMPLE_EXPERIMENTAL_CORO_EXCEPTION_MONITOR_H
#define ASYNC_SIMPLE_EXPERIMENTAL_CORO_EXCEPTION_MONITOR_H

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include <chrono>
#include <sstream>
#include <iomanip>

namespace async_simple {
namespace experimental {

// Coroutine context information for exception diagnosis
struct CoroutineContext {
    uint64_t coro_id;
    std::chrono::time_point<std::chrono::system_clock> create_time;
    std::string create_stack;
    std::string current_stack;
    std::unordered_map<std::string, std::string> resource_info;

    std::string toString() const {
        std::ostringstream oss;
        oss << "Coroutine ID: " << coro_id << std::endl;
        oss << "Create Time: " << std::put_time(std::localtime(&create_time.time_since_epoch().count()), "%Y-%m-%d %H:%M:%S") << std::endl;
        oss << "Create Stack:" << std::endl << create_stack << std::endl;
        oss << "Current Stack:" << std::endl << current_stack << std::endl;
        oss << "Resource Info:" << std::endl;
        for (const auto& [key, value] : resource_info) {
            oss << "  " << key << ": " << value << std::endl;
        }
        return oss.str();
    }
};

// Exception handler type
typedef std::function<void(const std::exception&, const CoroutineContext&)> CoroExceptionHandler;

// Coroutine exception monitor singleton
class CoroExceptionMonitor {
public:
    // Get singleton instance
    static CoroExceptionMonitor& instance() {
        static CoroExceptionMonitor monitor;
        return monitor;
    }

    // Delete copy and move constructors
    CoroExceptionMonitor(const CoroExceptionMonitor&) = delete;
    CoroExceptionMonitor& operator=(const CoroExceptionMonitor&) = delete;
    CoroExceptionMonitor(CoroExceptionMonitor&&) = delete;
    CoroExceptionMonitor& operator=(CoroExceptionMonitor&&) = delete;

    // Register a global exception handler
    void registerGlobalHandler(CoroExceptionHandler handler) {
        std::lock_guard<std::mutex> lock(handlers_mutex_);
        handlers_.push_back(std::move(handler));
    }

    // Unregister all global exception handlers
    void unregisterAllHandlers() {
        std::lock_guard<std::mutex> lock(handlers_mutex_);
        handlers_.clear();
    }

    // Handle an uncaught exception from a coroutine
    void handleException(const std::exception& e, const CoroutineContext& context) {
        std::lock_guard<std::mutex> lock(handlers_mutex_);
        for (const auto& handler : handlers_) {
            try {
                handler(e, context);
            } catch (const std::exception& handler_e) {
                // Ignore exceptions from handlers to prevent infinite loops
            }
        }
    }

    // Generate a unique coroutine ID
    uint64_t generateCoroId() {
        return coro_id_counter_.fetch_add(1, std::memory_order_relaxed);
    }

private:
    // Private constructor for singleton
    CoroExceptionMonitor() : coro_id_counter_(0) {}

    // Global exception handlers
    std::vector<CoroExceptionHandler> handlers_;
    std::mutex handlers_mutex_;

    // Coroutine ID counter
    std::atomic<uint64_t> coro_id_counter_;
};

// Helper macro to get current coroutine ID
#define CURRENT_CORO_ID() async_simple::experimental::CoroExceptionMonitor::instance().generateCoroId()

// Helper macro to record coroutine context
#define RECORD_CORO_CONTEXT(context) \
    do { \
        context.coro_id = CURRENT_CORO_ID(); \
        context.create_time = std::chrono::system_clock::now(); \
        // TODO: Implement stack trace capture \
        context.create_stack = "Stack trace not implemented"; \
        context.current_stack = "Stack trace not implemented"; \
    } while (0)

}  // namespace experimental
}  // namespace async_simple

#endif  // ASYNC_SIMPLE_EXPERIMENTAL_CORO_EXCEPTION_MONITOR_H
