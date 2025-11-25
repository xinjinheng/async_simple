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
#ifndef ASYNC_SIMPLE_CORO_COROUTINE_RESOURCE_GUARD_H
#define ASYNC_SIMPLE_CORO_COROUTINE_RESOURCE_GUARD_H

#include <memory>
#include <string>
#include "async_simple/Common.h"
#include "async_simple/Executor.h"
#include "async_simple/Promise.h"

namespace async_simple {
namespace coro {

class CoroutineResourceGuard {
public:
    CoroutineResourceGuard() = default;
    ~CoroutineResourceGuard() = default;

    // Disable copy
    CoroutineResourceGuard(const CoroutineResourceGuard&) = delete;
    CoroutineResourceGuard& operator=(const CoroutineResourceGuard&) = delete;

    // Enable move
    CoroutineResourceGuard(CoroutineResourceGuard&&) noexcept = default;
    CoroutineResourceGuard& operator=(CoroutineResourceGuard&&) noexcept = default;

    // Set executor
    void setExecutor(Executor* executor) {
        _executor = executor;
    }

    // Get executor (with validation)
    Executor* getExecutor(const std::string& accessLocation) const {
        AS_SAFE_ACCESS(_executor, "Executor", accessLocation);
        return _executor;
    }

    // Set promise
    template <typename T>
    void setPromise(Promise<T>* promise) {
        _promise = promise;
    }

    // Get promise (with validation)
    template <typename T>
    Promise<T>* getPromise(const std::string& accessLocation) const {
        auto promise = static_cast<Promise<T>*>(_promise);
        AS_SAFE_ACCESS(promise, "Promise", accessLocation);
        return promise;
    }

    // Check if all resources are valid
    bool isValid() const {
        return _executor != nullptr && _promise != nullptr;
    }

private:
    Executor* _executor = nullptr;
    void* _promise = nullptr;  // Type-erased promise pointer
};

}  // namespace coro
}  // namespace async_simple

#endif  // ASYNC_SIMPLE_CORO_COROUTINE_RESOURCE_GUARD_H
