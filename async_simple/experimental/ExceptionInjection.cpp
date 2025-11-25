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
#include "ExceptionInjection.h"
#include "NetworkException.h"
#include <stdexcept>
#include <chrono>

namespace async_simple {
namespace experimental {

ExceptionInjectionManager::ExceptionInjectionManager()
    : _distribution(0.0, 1.0) {
    // Initialize random engine with current time
    auto seed = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    _randomEngine.seed(static_cast<unsigned int>(seed));
}

ExceptionInjectionManager& ExceptionInjectionManager::instance() {
    static ExceptionInjectionManager instance;
    return instance;
}

void ExceptionInjectionManager::setConfig(const ExceptionInjectionConfig& config) {
    std::lock_guard<std::mutex> lock(_mutex);
    _config = config;
}

const ExceptionInjectionConfig& ExceptionInjectionManager::getConfig() const {
    std::lock_guard<std::mutex> lock(_mutex);
    return _config;
}

bool ExceptionInjectionManager::shouldInjectException() const {
    std::lock_guard<std::mutex> lock(_mutex);
    if (!_config.injectExecutorRelease && !_config.injectNetworkTimeout && !_config.injectResourceAccessConflict) {
        return false;
    }
    return _distribution(_randomEngine) < _config.injectionProbability;
}

void ExceptionInjectionManager::injectExecutorReleaseException() const {
    throw std::runtime_error("Executor released unexpectedly");
}

void ExceptionInjectionManager::injectNetworkTimeoutException() const {
    throw ConnectionTimeoutException("Network operation timed out");
}

void ExceptionInjectionManager::injectResourceAccessConflictException() const {
    throw std::runtime_error("Resource access conflict detected");
}

}  // namespace experimental
}  // namespace async_simple
