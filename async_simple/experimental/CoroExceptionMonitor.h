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
#include <exception>

namespace async_simple {
namespace experimental {

// Coroutine context information for exception debugging
struct CoroutineContext {
    std::string coroId;
    std::string createTime;
    std::string callStack;
    std::string associatedResources;
};

// Coroutine exception handler type
using CoroExceptionHandler = std::function<void(const std::exception_ptr&, const CoroutineContext&)>;

// Singleton component for global coroutine exception monitoring
class CoroExceptionMonitor {
public:
    // Get singleton instance
    static CoroExceptionMonitor& instance();
    
    // Register a global exception handler
    void registerGlobalHandler(CoroExceptionHandler handler);
    
    // Unregister all global exception handlers
    void unregisterAllHandlers();
    
    // Handle an uncaught exception from a coroutine
    void handleException(const std::exception_ptr& eptr, const CoroutineContext& context);
    
private:
    // Private constructor for singleton
    CoroExceptionMonitor() = default;
    
    // Private destructor
    ~CoroExceptionMonitor() = default;
    
    // Disable copy and move
    CoroExceptionMonitor(const CoroExceptionMonitor&) = delete;
    CoroExceptionMonitor& operator=(const CoroExceptionMonitor&) = delete;
    CoroExceptionMonitor(CoroExceptionMonitor&&) = delete;
    CoroExceptionMonitor& operator=(CoroExceptionMonitor&&) = delete;
    
private:
    std::mutex _mutex;
    CoroExceptionHandler _globalHandler;
};

}  // namespace experimental
}  // namespace async_simple

#endif  // ASYNC_SIMPLE_EXPERIMENTAL_CORO_EXCEPTION_MONITOR_H
