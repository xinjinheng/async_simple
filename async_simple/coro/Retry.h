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

#ifndef ASYNC_SIMPLE_CORO_RETRY_H
#define ASYNC_SIMPLE_CORO_RETRY_H

#include <chrono>
#include <functional>
#include <optional>
#include <type_traits>

#include "async_simple/coro/Lazy.h"
#include "async_simple/coro/Timeout.h"

namespace async_simple {
namespace coro {

// Retry policy base class
class RetryPolicy {
public:
    virtual ~RetryPolicy() = default;
    // Return true if we should retry, false otherwise
    virtual bool shouldRetry(size_t attempt, std::exception_ptr ex) const = 0;
    // Return the delay before next retry
    virtual std::chrono::milliseconds getDelay(size_t attempt) const = 0;
};

// No retry policy
class NoRetryPolicy : public RetryPolicy {
public:
    bool shouldRetry(size_t attempt, std::exception_ptr ex) const override {
        return false;
    }
    std::chrono::milliseconds getDelay(size_t attempt) const override {
        return std::chrono::milliseconds(0);
    }
};

// Exponential backoff retry policy
class ExponentialBackoffPolicy : public RetryPolicy {
public:
    ExponentialBackoffPolicy(std::chrono::milliseconds initialDelay,
                             std::chrono::milliseconds maxDelay,
                             size_t maxAttempts = 3,
                             double multiplier = 2.0)
        : _initialDelay(initialDelay),
          _maxDelay(maxDelay),
          _maxAttempts(maxAttempts),
          _multiplier(multiplier) {
    }

    bool shouldRetry(size_t attempt, std::exception_ptr ex) const override {
        return attempt < _maxAttempts;
    }

    std::chrono::milliseconds getDelay(size_t attempt) const override {
        auto delay = _initialDelay * std::pow(_multiplier, attempt);
        return std::min(delay, _maxDelay);
    }

private:
    std::chrono::milliseconds _initialDelay;
    std::chrono::milliseconds _maxDelay;
    size_t _maxAttempts;
    double _multiplier;
};

// Retry result structure
template <typename T>
struct RetryResult {
    T value;
    size_t attempts;
    bool succeeded;
};

namespace detail {

// Retry implementation for Lazy
// This function retries the given Lazy coroutine according to the policy
// It also handles timeout for each attempt if a timeout is specified

template <typename T, typename Policy, typename Func>
Lazy<RetryResult<T>> retryImpl(Func&& func, Policy&& policy,
                               std::optional<std::chrono::milliseconds> timeout,
                               Executor* ex = nullptr) {
    static_assert(std::is_invocable_r_v<Lazy<T>, Func&&>,
                  "Func must return Lazy<T>");
    
    size_t attempt = 0;
    while (true) {
        try {
            if (timeout) {
                // With timeout for each attempt
                auto result = co_await timeout(std::forward<Func>(func)(), *timeout, ex);
                if (result) {
                    co_return RetryResult<T>{std::move(*result), attempt + 1, true};
                }
                // Timeout occurred, check if we should retry
            } else {
                // Without timeout
                auto result = co_await std::forward<Func>(func)();
                co_return RetryResult<T>{std::move(result), attempt + 1, true};
            }
        } catch (...) {
            // Exception occurred, check if we should retry
        }
        
        if (!policy.shouldRetry(attempt, std::current_exception())) {
            break;
        }
        
        // Delay before next retry
        auto delay = policy.getDelay(attempt);
        if (delay.count() > 0) {
            co_await sleep(delay, ex);
        }
        
        attempt++;
    }
    
    // Failed after all attempts
    co_await suspendAlways{};
    // This line should never be reached
    std::rethrow_exception(std::current_exception());
}

}  // namespace detail

// Retry a Lazy coroutine with a policy and optional per-attempt timeout
template <typename Func, typename Policy = ExponentialBackoffPolicy,
          typename T = typename std::invoke_result_t<Func&&>::value_type>
Lazy<RetryResult<T>> retry(Func&& func, Policy&& policy = Policy(),
                           std::optional<std::chrono::milliseconds> timeout = std::nullopt,
                           Executor* ex = nullptr) {
    return detail::retryImpl<T>(std::forward<Func>(func),
                               std::forward<Policy>(policy), timeout, ex);
}

// Retry with exponential backoff and default parameters
template <typename Func,
          typename T = typename std::invoke_result_t<Func&&>::value_type>
Lazy<RetryResult<T>> retryExponential(Func&& func,
                                      std::chrono::milliseconds initialDelay = std::chrono::milliseconds(100),
                                      std::chrono::milliseconds maxDelay = std::chrono::milliseconds(1000),
                                      size_t maxAttempts = 3,
                                      std::optional<std::chrono::milliseconds> timeout = std::nullopt,
                                      Executor* ex = nullptr) {
    return retry(std::forward<Func>(func),
                ExponentialBackoffPolicy(initialDelay, maxDelay, maxAttempts),
                timeout, ex);
}

}  // namespace coro
}  // namespace async_simple

#endif  // ASYNC_SIMPLE_CORO_RETRY_H