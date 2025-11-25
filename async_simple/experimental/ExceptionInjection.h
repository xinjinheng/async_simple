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
#ifndef ASYNC_SIMPLE_EXPERIMENTAL_EXCEPTION_INJECTION_H
#define ASYNC_SIMPLE_EXPERIMENTAL_EXCEPTION_INJECTION_H

#include <functional>
#include <random>
#include <atomic>

namespace async_simple {
namespace experimental {

// Exception injection configuration
struct ExceptionInjectionConfig {
    bool injectExecutorRelease = false;
    bool injectNetworkTimeout = false;
    bool injectResourceAccessConflict = false;
    double injectionProbability = 0.1;  // 10% probability by default
};

// Exception injection manager
class ExceptionInjectionManager {
public:
    // Get singleton instance
    static ExceptionInjectionManager& instance();
    
    // Set configuration
    void setConfig(const ExceptionInjectionConfig& config);
    
    // Get current configuration
    const ExceptionInjectionConfig& getConfig() const;
    
    // Check if we should inject an exception based on current configuration
    bool shouldInjectException() const;
    
    // Inject executor release exception
    void injectExecutorReleaseException() const;
    
    // Inject network timeout exception
    void injectNetworkTimeoutException() const;
    
    // Inject resource access conflict exception
    void injectResourceAccessConflictException() const;
    
private:
    // Private constructor for singleton
    ExceptionInjectionManager();
    
    // Private destructor
    ~ExceptionInjectionManager() = default;
    
    // Disable copy and move
    ExceptionInjectionManager(const ExceptionInjectionManager&) = delete;
    ExceptionInjectionManager& operator=(const ExceptionInjectionManager&) = delete;
    ExceptionInjectionManager(ExceptionInjectionManager&&) = delete;
    ExceptionInjectionManager& operator=(ExceptionInjectionManager&&) = delete;
    
private:
    mutable std::mutex _mutex;
    ExceptionInjectionConfig _config;
    mutable std::mt19937 _randomEngine;
    mutable std::uniform_real_distribution<double> _distribution;
};

// Macro to inject exceptions at specific points
#define AS_INJECT_EXCEPTION() \
    do { \
        auto& manager = async_simple::experimental::ExceptionInjectionManager::instance(); \
        if (manager.shouldInjectException()) { \
            if (manager.getConfig().injectExecutorRelease) { \
                manager.injectExecutorReleaseException(); \
            } else if (manager.getConfig().injectNetworkTimeout) { \
                manager.injectNetworkTimeoutException(); \
            } else if (manager.getConfig().injectResourceAccessConflict) { \
                manager.injectResourceAccessConflictException(); \
            } \
        } \
    } while (false)

}  // namespace experimental
}  // namespace async_simple

#endif  // ASYNC_SIMPLE_EXPERIMENTAL_EXCEPTION_INJECTION_H
