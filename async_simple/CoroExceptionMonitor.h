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
#ifndef ASYNC_SIMPLE_CORO_EXCEPTION_MONITOR_H
#define ASYNC_SIMPLE_CORO_EXCEPTION_MONITOR_H

#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <mutex>
#include <exception>
#include <async_simple/coro/Lazy.h>

namespace async_simple {

// Coroutine exception context
struct CoroExceptionContext {
    uint64_t coroId;                  // Coroutine ID
    std::string coroutineType;        // Coroutine type (Lazy, Future, etc.)
    std::string exceptionType;        // Type of the exception
    std::string exceptionMessage;     // Exception message
    std::vector<std::string> callStack; // Call stack snapshot
    std::vector<std::string> resourceList; // List of associated resources
    uint64_t createTime;              // Coroutine creation time (timestamp)
    uint64_t exceptionTime;           // Exception occurrence time (timestamp)
};

// Coroutine exception handler type
typedef std::function<void(const CoroExceptionContext&)> CoroExceptionHandler;

// Coroutine exception monitor singleton
class CoroExceptionMonitor {
public:
    // Get the singleton instance
    static CoroExceptionMonitor& instance();
    
    // Delete copy and move constructors
    CoroExceptionMonitor(const CoroExceptionMonitor&) = delete;
    CoroExceptionMonitor& operator=(const CoroExceptionMonitor&) = delete;
    CoroExceptionMonitor(CoroExceptionMonitor&&) = delete;
    CoroExceptionMonitor& operator=(CoroExceptionMonitor&&) = delete;
    
    // Register a global exception handler
    void registerGlobalHandler(CoroExceptionHandler handler);
    
    // Unregister a global exception handler
    void unregisterGlobalHandler(CoroExceptionHandler handler);
    
    // Handle an uncaught exception from a coroutine
    void handleException(const CoroExceptionContext& context);
    
    // Create an exception context for the current coroutine
    CoroExceptionContext createExceptionContext(
        const std::exception_ptr& ex,
        const std::string& coroutineType = "Unknown");
    
private:
    // Private constructor
    CoroExceptionMonitor();
    
    // Destructor
    ~CoroExceptionMonitor() = default;
    
    // Get current timestamp in milliseconds
    uint64_t getCurrentTimestamp() const;
    
    // Get call stack snapshot
    std::vector<std::string> getCallStack() const;
    
    // Get list of associated resources for current coroutine
    std::vector<std::string> getResourceList() const;
    
private:
    std::vector<CoroExceptionHandler> _handlers;
    std::mutex _handlersMutex;
};

// Helper function to handle uncaught coroutine exceptions
inline void handleCoroutineException(const std::exception_ptr& ex,
                                     const std::string& coroutineType = "Unknown") {
    auto context = CoroExceptionMonitor::instance().createExceptionContext(ex, coroutineType);
    CoroExceptionMonitor::instance().handleException(context);
}

}  // namespace async_simple

#endif  // ASYNC_SIMPLE_CORO_EXCEPTION_MONITOR_H
